#!/usr/bin/env python3
"""Fail-closed tests for source selection and incomplete STMM qualification."""
import copy
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

import stmm_ci as ci


class StmmCiTests(unittest.TestCase):
    def setUp(self):
        self.policy = json.loads(ci.POLICY.read_text())
        self.case = self.policy['cases'][0]
        self.scratch = ci.ROOT / '.worktrees' / ('session-' + os.environ.get('AGENT_SESSION_ID', 'stmm-ci-test'))
        self.scratch.mkdir(parents=True, exist_ok=True)
        self.tmp = tempfile.TemporaryDirectory(dir=self.scratch, prefix='stmm-ci-test-')
        self.addCleanup(self.tmp.cleanup)
        self.output = Path(self.tmp.name)
        self.identity = {'key': self.case['key'], 'source': self.case['candidate'], 'input_state': 'qualified-candidate'}

    def result(self):
        result = ci.new_result(self.policy, self.case['key'], 'candidate', 'preflight')
        receipt = (json.dumps(self.identity, indent=2, sort_keys=True) + '\n').encode()
        result.update(subject=self.identity, status='passed',
                      evidence_sha256={'inputs.json': ci.sha(receipt)})
        result['criteria']['input-validation'] = 'passed'
        return result

    def summary(self, rows):
        for index, row in enumerate(rows):
            folder = self.output / str(index)
            folder.mkdir()
            ci.save(folder / 'inputs.json', self.identity)
            ci.save(folder / 'result.json', row)
        policy = dict(self.policy, cases=[self.case])
        with patch.object(ci, 'subject', return_value=self.identity):
            return ci.aggregate(policy, 'candidate', 'preflight', self.output, {'plan': 'success', 'cases': 'success', 'download': 'success'})

    def test_partial_success_never_qualifies_a_descendant(self):
        result = self.summary([self.result()])
        self.assertEqual(result['status'], 'passed')
        self.assertEqual(result['qualification'], 'incomplete')
        self.assertIn('minimised-clone-rebuild-replay', result['outstanding'])
        self.assertIn('paired-cuts-189-149-40', result['outstanding'])
        self.assertIn('integrated-unqualified', result['qualification_states'])

    def test_missing_case_fails_instead_of_becoming_a_pass(self):
        self.assertEqual(self.summary([])['status'], 'failed')

    def test_duplicate_case_is_not_hidden_by_last_result(self):
        self.assertEqual(self.summary([self.result(), self.result()])['status'], 'failed')

    def test_stale_identity_and_attempt_are_rejected(self):
        row = self.result()
        row['subject'] = dict(self.identity, source={'commit': '0' * 40})
        self.assertEqual(self.summary([row])['status'], 'failed')

    def test_other_run_is_rejected(self):
        row = self.result()
        row['run_id'] = 'different-run'
        self.assertEqual(self.summary([row])['status'], 'failed')

    def test_claiming_qualified_after_preflight_is_rejected(self):
        row = self.result()
        row['qualification'] = 'qualified-descendant'
        self.assertEqual(self.summary([row])['status'], 'failed')

    def test_passing_status_without_required_stage_evidence_is_rejected(self):
        row = self.result()
        row['criteria']['input-validation'] = 'unverified'
        self.assertEqual(self.summary([row])['status'], 'failed')

    def test_blocked_failed_incomplete_and_unverified_are_not_passes(self):
        for status in ('blocked', 'failed', 'incomplete', 'unverified'):
            with self.subTest(status=status):
                row = self.result()
                row['status'] = status
                with patch.object(ci, 'subject', return_value=self.identity):
                    folder = self.output / status
                    folder.mkdir()
                    ci.save(folder / 'result.json', row)
                    result = ci.aggregate(dict(self.policy, cases=[self.case]), 'candidate', 'preflight',
                                          folder, {'plan': 'success', 'cases': 'success', 'download': 'success'})
                    self.assertEqual(result['status'], 'failed')

    def test_502_does_not_silently_substitute_a_maintained_source(self):
        case = next(c for c in self.policy['cases'] if c['edk2'] == '202502')
        with self.assertRaisesRegex(ValueError, 'no full-source candidate'):
            ci.subject(self.policy, case, 'candidate')

    def test_moved_maintained_ref_is_rejected(self):
        with patch.object(ci, 'object_pin'), patch.object(ci, 'git', return_value='f' * 40):
            with self.assertRaisesRegex(ValueError, 'ref moved'):
                ci.subject(self.policy, self.case, 'maintained')

    def test_local_image_id_cannot_trigger_a_compile(self):
        policy = copy.deepcopy(self.policy)
        policy['compiler_image'] = 'sha256:' + 'a' * 64
        with patch.object(ci.subprocess, 'run') as run:
            with self.assertRaisesRegex(ValueError, 'immutable OCI'):
                ci.compile_native(policy, self.identity, self.output)
            run.assert_not_called()

    def test_failed_workflow_dependency_stays_failed(self):
        ci.save(self.output / 'result.json', self.result())
        with patch.object(ci, 'subject', return_value=self.identity):
            result = ci.aggregate(dict(self.policy, cases=[self.case]), 'candidate', 'preflight',
                                  self.output, {'cases': 'failure'})
        self.assertEqual(result['status'], 'failed')

    def test_corrupt_artifact_is_rejected(self):
        row = self.result()
        row['evidence_sha256'] = {'inputs.json': ci.sha(b'original')}
        folder = self.output / '0'
        folder.mkdir()
        (folder / 'inputs.json').write_bytes(b'corrupted')
        ci.save(folder / 'result.json', row)
        with patch.object(ci, 'subject', return_value=self.identity):
            result = ci.aggregate(dict(self.policy, cases=[self.case]), 'candidate', 'preflight',
                                  self.output, {'plan': 'success', 'cases': 'success', 'download': 'success'})
        self.assertEqual(result['status'], 'failed')

    def test_malformed_result_is_reported_as_failure(self):
        (self.output / 'result.json').write_text('{invalid json')
        result = ci.aggregate(dict(self.policy, cases=[self.case]), 'candidate', 'preflight',
                              self.output, {'plan': 'success', 'cases': 'success', 'download': 'success'})
        self.assertEqual(result['status'], 'failed')

    def test_missing_input_receipt_cannot_pass(self):
        row = self.result()
        row['evidence_sha256'] = {}
        self.assertEqual(self.summary([row])['status'], 'failed')

    def test_unevaluated_criteria_cannot_claim_success(self):
        row = self.result()
        row['criteria']['cold-persistence'] = 'passed'
        self.assertEqual(self.summary([row])['status'], 'failed')

    def test_missing_workflow_dependencies_cannot_pass(self):
        ci.save(self.output / 'inputs.json', self.identity)
        ci.save(self.output / 'result.json', self.result())
        with patch.object(ci, 'subject', return_value=self.identity):
            result = ci.aggregate(dict(self.policy, cases=[self.case]), 'candidate', 'preflight',
                                  self.output, {'cases': 'success'})
        self.assertEqual(result['status'], 'failed')

    def test_non_scalar_status_fails_closed(self):
        row = self.result()
        row['status'] = []
        self.assertEqual(self.summary([row])['status'], 'failed')

    def test_manifest_cannot_silently_weaken_acceptance(self):
        for field, value in [('required_criteria', ['input-validation']),
                             ('blocked_criteria', []), ('minimum_disk_gib', 0),
                             ('cases', []), ('cases', [self.case, self.case])]:
            with self.subTest(field=field):
                with self.assertRaises(ValueError):
                    ci.validate_policy(dict(self.policy, **{field: value}))

    def test_preflight_with_only_publication_adapter_files(self):
        # Model a clean publication lacking every native WIP module.
        scripts = self.output / 'scripts'
        scripts.mkdir()
        for name in ci.HELPERS + ci.TESTS:
            (scripts / name).write_bytes(b'published adapter')
        config = self.output / 'config'
        config.mkdir()
        entry = dict(self.case['maintained'], object_id=self.case['maintained']['commit'],
                     tree_id=self.case['maintained']['tree'], edk2_base='edk2-stable' + self.case['edk2'],
                     radxa_release=self.case['radxa'])
        ci.save(config / 'refs-unofficial.json', {'refs': [entry]})
        policy_path = config / 'stmm-ci.json'
        ci.save(policy_path, self.policy)
        refs = {pin['ref']: pin['commit'] for pin in self.policy['components']['maintained'].values()}
        refs[self.case['maintained']['ref']] = self.case['maintained']['commit']

        def resolve(*args):
            return refs.get(args[-1], 'a' * 40)

        with patch.object(ci, 'ROOT', self.output), patch.object(ci, 'POLICY', policy_path):
            with patch.object(ci, 'object_pin'), patch.object(ci, 'git', side_effect=resolve):
                result = ci.subject(self.policy, self.case, 'maintained')
                with patch.object(ci, 'compile_native') as compiler, patch('builtins.print'):
                    status = ci.run_case(self.policy, self.case['key'], 'maintained', 'preflight',
                                         self.output / 'preflight')
                compiler.assert_not_called()
                self.assertEqual(status, 0)
        self.assertEqual(set(result['helpers']), {'stmm_ci.py'})
        self.assertEqual(set(result['test_suite']), {'test_stmm_ci.py'})

    def test_native_identity_covers_every_required_test(self):
        # Hashing native inputs is explicit and only requested for this stage.
        with patch.object(ci, 'object_pin'), patch.object(ci, 'git', return_value='a' * 40), patch.object(
                ci, 'file_hashes', side_effect=lambda names: {name: 'a' * 64 for name in names}):
            result = ci.subject(self.policy, self.case, 'candidate', 'native-compile')
        self.assertEqual(set(result['test_suite']), set(ci.TESTS + ci.NATIVE_TESTS))
        self.assertEqual(set(result['helpers']), set(ci.HELPERS + ci.NATIVE_HELPERS))

    def test_not_integrated_source_blocks_native_compilation(self):
        identity = dict(self.identity, input_state='not-integrated')
        with patch.object(ci, 'subject', return_value=identity), patch.object(ci, 'compile_native') as compiler:
            with patch('builtins.print'):
                status = ci.run_case(self.policy, self.case['key'], 'maintained', 'native-compile',
                                     self.output / 'blocked-native')
        self.assertEqual(status, 1)
        compiler.assert_not_called()
        result = json.loads((self.output / 'blocked-native/result.json').read_text())
        self.assertEqual(result['status'], 'blocked')
        self.assertEqual(result['qualification'], 'incomplete')

    def test_native_stage_requires_complete_build_evidence(self):
        row = self.result()
        row['stage'] = 'native-compile'
        row['criteria']['native-compile'] = 'passed'
        ci.save(self.output / 'inputs.json', self.identity)
        ci.save(self.output / 'result.json', row)
        with patch.object(ci, 'subject', return_value=self.identity):
            result = ci.aggregate(dict(self.policy, cases=[self.case]), 'candidate', 'native-compile',
                                  self.output, {'plan': 'success', 'cases': 'success', 'download': 'success'})
        self.assertEqual(result['status'], 'failed')

    def test_candidate_cannot_claim_descendant_state(self):
        case = copy.deepcopy(self.case)
        case['candidate']['state'] = 'qualified-descendant'
        with self.assertRaisesRegex(ValueError, 'invalid state'):
            ci.subject(self.policy, case, 'candidate')

    def test_failed_preflight_writes_a_result_without_building(self):
        output = self.output / 'attempt'
        with patch.object(ci, 'subject', side_effect=ValueError('missing inputs')):
            with patch.object(ci, 'compile_native') as compile_native:
                with patch('builtins.print'):
                    status = ci.run_case(self.policy, self.case['key'], 'candidate', 'preflight', output)
        compile_native.assert_not_called()
        self.assertEqual(status, 1)
        self.assertEqual(json.loads((output / 'result.json').read_text())['status'], 'blocked')
        with self.assertRaises(FileExistsError):
            ci.run_case(self.policy, self.case['key'], 'candidate', 'preflight', output)


class NativeWorkflowTests(unittest.TestCase):
    def setUp(self):
        workflow = (ci.ROOT / '.github/workflows/stmm-release-matched.yaml').read_text()
        cases = workflow.split('\n  cases:\n', 1)[1].split('\n  summary:\n', 1)[0]
        self.steps = re.split(r'^      - ', cases, flags=re.M)[1:]
        self.fetch = next(i for i, step in enumerate(self.steps) if 'name: Fetch required source refs' in step)
        self.setup = next(i for i, step in enumerate(self.steps) if 'name: Check exact harness' in step)
        self.gate = next(i for i, step in enumerate(self.steps) if 'name: Native preparation regressions' in step)
        self.compile = next(i for i, step in enumerate(self.steps) if 'name: Validate inputs or compile' in step)

    @staticmethod
    def script(step):
        value = step.split('        run: |\n', 1)[1]
        return '\n'.join(line[10:] for line in value.splitlines()) + '\n'

    def test_native_tests_follow_fetch_identity_and_scratch_setup(self):
        self.assertLess(self.fetch, self.setup)
        self.assertLess(self.setup, self.gate)
        self.assertLess(self.gate, self.compile)
        setup = self.steps[self.setup]
        for guard in ('${WORKFLOW_SHA}', '$(git rev-parse HEAD)', '${REQUESTED_SHA}',
                      'TMPDIR TMP TEMP EDK2_CIX_TMP_ROOT'):
            self.assertIn(guard, setup)
        gate = self.steps[self.gate]
        self.assertIn("if: ${{ inputs.stage == 'native-compile' }}", gate)
        self.assertNotIn('continue-on-error', gate)
        self.assertNotIn('if:', self.steps[self.compile])
        self.assertNotIn('continue-on-error', self.steps[self.compile])
        executed = re.findall(r'-p (test_\w+\.py)', self.script(gate))
        self.assertEqual(executed, list(ci.NATIVE_TESTS))
        self.assertEqual(set(executed), {'test_stmm_release_platform.py', 'test_stmm_release_port.py',
                                         'test_stmm_vendor_control.py'})

    def test_failure_in_any_native_suite_stops_before_compilation(self):
        # Execute the actual workflow shell blocks with a Python command stub:
        # no compiler, Docker, source import or native suite runs in this test.
        for failure in (*ci.NATIVE_TESTS, ''):
            with self.subTest(failure=failure), tempfile.TemporaryDirectory(prefix='stmm-gate-') as temporary:
                root = Path(temporary)
                stub = root / 'python3'
                stub.write_text('#!' + sys.executable + '\n'
                                'import os, sys\n'
                                'from pathlib import Path\n'
                                'name = sys.argv[-1] if "unittest" in sys.argv else "compile"\n'
                                'with Path(os.environ["STMM_TEST_LOG"]).open("a") as log:\n'
                                '    log.write(name + "\\n")\n'
                                'sys.exit(9 if name == os.environ["STMM_FAIL_TEST"] else 0)\n')
                stub.chmod(0o755)
                log = root / 'calls'
                env = dict(os.environ, PATH=str(root) + os.pathsep + os.environ['PATH'],
                           STMM_TEST_LOG=str(log), STMM_FAIL_TEST=failure, STMM_ROOT=str(root),
                           CASE_KEY='test', SELECTION='candidate', STAGE='native-compile')
                script = self.script(self.steps[self.gate]) + self.script(self.steps[self.compile])
                result = subprocess.run(['bash', '-c', script], env=env, capture_output=True, text=True, timeout=10)
                calls = log.read_text().splitlines()
                if failure:
                    self.assertEqual(result.returncode, 9, result.stderr)
                    self.assertEqual(calls, list(ci.NATIVE_TESTS[:ci.NATIVE_TESTS.index(failure) + 1]))
                    self.assertNotIn('compile', calls)
                else:
                    self.assertEqual(result.returncode, 0, result.stderr)
                    self.assertEqual(calls, list(ci.NATIVE_TESTS) + ['compile'])

    def test_preflight_plan_has_no_native_test_or_tool_dependency(self):
        workflow = (ci.ROOT / '.github/workflows/stmm-release-matched.yaml').read_text()
        plan = workflow.split('\n  plan:\n', 1)[1].split('\n  cases:\n', 1)[0]
        self.assertIn('-p test_stmm_ci.py', plan)
        for name in ci.NATIVE_HELPERS + ci.NATIVE_TESTS:
            self.assertNotIn(name, plan)
        self.assertNotIn('docker', plan)


if __name__ == '__main__':
    unittest.main()
