#!/usr/bin/env python3
"""Exercise real HII callback dispatch and browser storage loading with EFI mocks.

SOURCE_TEST_REF/SOURCE_TEST_ROOT select the firmware source, as in the existing
setup tests. The harness compiles the selected production functions with
mock EFI interfaces.
"""
import re
import unittest

from test_firmware_update_safety import function, run_c, source as raw_source


def source(path):
    return raw_source(path).replace('\r\n', '\n')


SETUP = 'src/edk2/MdeModulePkg/Universal/SetupBrowserDxe/'
FRONT = 'src/edk2/MdeModulePkg/Application/UiApp/FrontPage.c'

PRELUDE = r'''
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <stdio.h>
#define IN
#define OUT
#define CONST const
#define VOID void
#define EFIAPI
#define TRUE 1
#define FALSE 0
#define EFI_SUCCESS 0
#define EFI_BUFFER_TOO_SMALL 2
#define EFI_NOT_FOUND 14
#define EFI_DEVICE_ERROR 7
#define EFI_ERROR(x) ((x) != 0)
#define ASSERT(x) assert(x)
#define DEBUG(x) do {} while(0)
#define EFI_HII_VARSTORE_EFI_VARIABLE 0
#define EFI_HII_VARSTORE_EFI_VARIABLE_BUFFER 1
#define EFI_HII_VARSTORE_BUFFER 2
#define EFI_HII_VARSTORE_NAME_VALUE 3
#define EFI_HII_DEFAULT_CLASS_STANDARD 0
#define FormSetLevel 0
#define GetDefaultForStorage 0
#define StrSize(s) ((wcslen(s)+1)*sizeof(wchar_t))
#define StrStr wcsstr
#define FreePool free
#define AllocateZeroPool(n) calloc(1,n)
#define UnicodeSPrint(...) ((void)0)
typedef size_t UINTN;
typedef uint8_t UINT8;
typedef uint16_t UINT16;
typedef uint32_t UINT32;
typedef uint8_t BOOLEAN;
typedef wchar_t CHAR16;
typedef CHAR16 *EFI_STRING;
typedef void *EFI_HII_HANDLE;
typedef int EFI_STATUS;
typedef struct { UINT32 a; UINT16 b,c; UINT8 d[8]; } EFI_GUID;
typedef struct list { struct list *ForwardLink,*BackLink; } LIST_ENTRY;
typedef struct { LIST_ENTRY Link; CHAR16 *Name,*Value,*EditValue; } NAME_VALUE_NODE;
#define NAME_VALUE_NODE_FROM_LINK(p) ((NAME_VALUE_NODE *)(p))
static LIST_ENTRY *GetFirstNode(LIST_ENTRY *p) { return p->ForwardLink; }
static int IsNull(LIST_ENTRY *p,LIST_ENTRY *q) { return p==q; }
static LIST_ENTRY *GetNextNode(LIST_ENTRY *p,LIST_ENTRY *q) { (void)p;return q->ForwardLink; }
static void NewStringCpy(CHAR16 **p,CHAR16 *q) { (void)p;(void)q;assert(0); }
static void *AllocateCopyPool(size_t n,const void *p) { void *q=malloc(n);assert(q);memcpy(q,p,n);return q; }
'''


class SetupFormOpenTests(unittest.TestCase):
    def storage_case(self, stored_size, read_status=0):
        header = source(SETUP + 'Setup.h')
        match = re.search(r'typedef struct \{[^}]*\} BROWSER_STORAGE;', header)
        self.assertIsNotNone(match)
        body = PRELUDE + match.group() + r'''
typedef struct { BROWSER_STORAGE *BrowserStorage; CHAR16 *ConfigHdr,*ConfigRequest; UINTN ElementCount; } FORMSET_STORAGE;
typedef int FORM_BROWSER_FORMSET;
static size_t returned_size, input_capacity, copy_width;
static int get_status, defaults, overflow_attempt;
static UINT8 *tracked_source,*tracked_destination;
enum { CAPACITY=16 };
static void checked_copy(void *d,const void *s,size_t n) {
 if(s==tracked_source && d==tracked_destination) {
  copy_width=n;
  if(n>CAPACITY) { overflow_attempt=1;return; }
 }
 memcpy(d,s,n);
}
#define CopyMem checked_copy
#define ZeroMem(p,n) memset(p,0,n)
static int get(CHAR16 *n,EFI_GUID *g,void *a,UINTN *size,void *data) {
 (void)n;(void)g;(void)a;input_capacity=*size;
 if(get_status==EFI_NOT_FOUND || get_status==EFI_DEVICE_ERROR)return get_status;
 *size=returned_size;
 if(returned_size>input_capacity)return EFI_BUFFER_TOO_SMALL;
 memset(data,0x5a,returned_size);return 0;
}
static struct { int (*GetVariable)(CHAR16*,EFI_GUID*,void*,UINTN*,void*); } services={get},*gRT=&services;
static int routing_get(void *self,CHAR16 *req,CHAR16 **progress,CHAR16 **result) {
 (void)self;(void)req;(void)progress;
 /* Model the old router's local native-width size, bounded config conversion. */
 UINTN size=CAPACITY;UINT8 data[CAPACITY];int status=get(NULL,NULL,NULL,&size,data);
 if(!status)*result=AllocateCopyPool(sizeof(L"result"),L"result");
 return status;
}
static int unused_block(void *a,CHAR16 *b,UINT8 *c,UINTN d,CHAR16 **e,CHAR16 **f) {
 (void)a;(void)b;(void)c;(void)d;(void)e;(void)f;assert(0);return 1;
}
static int unused_config(void *a,CHAR16 *b,UINT8 *c,UINTN *d,CHAR16 **e) {
 (void)a;(void)b;(void)c;(void)d;(void)e;assert(0);return 1;
}
static struct {
 int (*ExtractConfig)(void*,CHAR16*,CHAR16**,CHAR16**);
 int (*BlockToConfig)(void*,CHAR16*,UINT8*,UINTN,CHAR16**,CHAR16**);
 int (*ConfigToBlock)(void*,CHAR16*,UINT8*,UINTN*,CHAR16**);
} routing={routing_get,unused_block,unused_config},*mHiiConfigRouting=&routing;
static void ConfigRequestAdjust(BROWSER_STORAGE *s,CHAR16 *r,int v) { (void)s;(void)r;(void)v;assert(0); }
static int ConfigRespToStorage(BROWSER_STORAGE *s,CHAR16 *r) { (void)r;memset(s->EditBuffer,0x5a,returned_size);return 0; }
static void ExtractDefault(FORM_BROWSER_FORMSET *f,void *a,int b,int c,int d,BROWSER_STORAGE *s,int e,int g) {
 (void)f;(void)a;(void)b;(void)c;(void)d;(void)e;(void)g;++defaults;
 /* Defaults are mocked: keep memory safe so the real synchronization is observed. */
 memset(s->EditBuffer,0,CAPACITY);
}
'''
        text = source(SETUP + 'Setup.c')
        load = function(text, 'LoadStorage')
        direct = 'gRT->GetVariable' in load
        body += function(text, 'SynchronizeStorage') + load
        body += r'''
int main(void) {
 /* The actual structure has native-width pointer alignment, as on AArch64. */
 assert(sizeof(UINTN)==8);
 BROWSER_STORAGE storage={0};FORMSET_STORAGE form={0};
 storage.Type=EFI_HII_VARSTORE_EFI_VARIABLE_BUFFER;storage.Size=CAPACITY;
 storage.Buffer=calloc(1,CAPACITY);storage.EditBuffer=calloc(1,CAPACITY);
 /* A short direct read must initialize the unread tail even with stale bytes. */
 if (DIRECT_READ && INPUT_SUCCESS) memset(storage.EditBuffer,0xcc,CAPACITY);
 storage.Name=L"fixture";form.BrowserStorage=&storage;
 form.ConfigHdr=L"GUID=fixture&NAME=fixture&PATH=fixture";form.ConfigRequest=L"request";
 tracked_source=storage.EditBuffer;tracked_destination=storage.Buffer;
'''
        successful_read = not read_status and stored_size <= 16
        body = f'#define DIRECT_READ {int(direct)}\n#define INPUT_SUCCESS {int(successful_read)}\n' + body
        body += f'returned_size={stored_size};get_status={read_status};\n'
        body += r'''
 LoadStorage(NULL,&form);
 fprintf(stderr,"capacity=%zu returned=%zu schema=%u sync=%zu overflow=%d defaults=%d\n",
  input_capacity,returned_size,storage.Size,copy_width,overflow_attempt,defaults);
 assert(!overflow_attempt);
 assert(storage.Size==CAPACITY);
 assert(input_capacity==CAPACITY);
 assert(copy_width==CAPACITY);
 assert(defaults==!INPUT_SUCCESS);
 for (size_t i=0;i<CAPACITY;i++) {
  UINT8 expected=(INPUT_SUCCESS && i<returned_size) ? 0x5a : 0;
  assert(storage.EditBuffer[i]==expected);
  assert(storage.Buffer[i]==expected);
 }
 storage.EditBuffer[CAPACITY-1]=0xff;
 tracked_source=storage.Buffer;tracked_destination=storage.EditBuffer;
 copy_width=0;
 assert(SynchronizeStorage(&storage,NULL,FALSE)==EFI_SUCCESS);
 assert(!overflow_attempt && copy_width==CAPACITY);
 assert(storage.EditBuffer[CAPACITY-1]==storage.Buffer[CAPACITY-1]);
 free(storage.ConfigRequest);free(storage.EditBuffer);free(storage.Buffer);
 return 0;
}
'''
        # The old routed implementation does not call gRT directly.
        if not direct:
            body = body.replace('} services={get},*gRT=&services;', '} services={get};')
            body = body.replace('static struct { int (*GetVariable)', 'struct { int (*GetVariable)')
        run_c(self, body)

    def test_exact_variable_preserves_schema_capacity(self):
        self.storage_case(16)

    def test_short_variable_preserves_schema_capacity(self):
        self.storage_case(4)

    def test_oversized_variable_never_synchronizes_beyond_allocation(self):
        self.storage_case(32)

    def test_absent_variable_falls_back_without_changing_capacity(self):
        self.storage_case(0, 14)

    def test_device_error_falls_back_without_changing_capacity(self):
        self.storage_case(0, 7)

    def test_main_form_retrieve_accepts_the_declared_numeric_question(self):
        platform = 'edk2-platforms/Platform/Radxa/Platforms/CIX/Sky1/Drivers/PlatformConfigDxe/'
        vfr = source('src/' + platform + 'PlatformConfigHii.vfr')
        header = source('src/' + platform + 'PlatformConfigHii.h')
        question = re.search(r'QUESTION_STATE_AFTER_G3_ID\s+(0x[0-9a-fA-F]+)', header).group(1)
        self.assertRegex(vfr, r'(?s)oneof varid\s*= PlatformSetupVar.StateAfterG3,.*?questionid\s*= QUESTION_STATE_AFTER_G3_ID,.*?flags\s*= NUMERIC_SIZE_1 \| INTERACTIVE')
        callback_path = 'custom/overlay/' + platform + 'PlatformConfigDxe.c'
        body = PRELUDE + r'''
#define EFI_UNSUPPORTED 3
#define EFI_INVALID_PARAMETER 6
#define EFI_BROWSER_ACTION_CHANGED 1
#define EFI_BROWSER_ACTION_CHANGING 0
#define EFI_BROWSER_ACTION_RETRIEVE 2
#define EFI_BROWSER_ACTION_FORM_OPEN 3
#define EFI_BROWSER_ACTION_FORM_CLOSE 4
#define EFI_BROWSER_ACTION_REQUEST_NONE 0
#define EFI_BROWSER_ACTION_REQUEST_RESET 1
#define EFI_BROWSER_ACTION_REQUEST_SUBMIT 2
#define EFI_BROWSER_ACTION_REQUEST_EXIT 3
#define EFI_BROWSER_ACTION_REQUEST_FORM_SUBMIT_EXIT 4
#define EFI_BROWSER_ACTION_REQUEST_FORM_DISCARD_EXIT 5
#define EFI_BROWSER_ACTION_REQUEST_FORM_APPLY 6
#define EFI_BROWSER_ACTION_REQUEST_FORM_DISCARD 7
#define EFI_BROWSER_ACTION_REQUEST_RECONNECT 8
#define EFI_BROWSER_ACTION_REQUEST_QUESTION_APPLY 9
#define EFI_IFR_TYPE_BOOLEAN 4
#define EFI_IFR_TYPE_STRING 7
#define EFI_IFR_TYPE_BUFFER 11
#define EFI_IFR_FLAG_CALLBACK 4
#define ExpressDisable 4
#define FormLevel 1
#define BROWSER_ACTION_DISCARD 1
#define BROWSER_RECONNECT_SAVE_CHANGES 1
#define BROWSER_RECONNECT_REQUIRED 2
#define GetSetValueWithHiiDriver 0
#define GetSetValueWithEditBuffer 1
#define FixedPcdGet8(x) 2
#define KEY_ENABLE_NETWORK_STACK 0x1900
#define KEY_DISABLE_ACPI_CPPC 0x1901
#define KEY_DISABLE_SMALL_CORE 0x1902
#define KEY_ENABLE_ACPI_LPI0 0x1903
#define NETWORK_STACK_VAR L"network"
#define PLATFORM_SETUP_VAR L"platform"
#define COMPLIANCE_VAR L"compliance"
#define CompareMem memcmp
#define CopyMem memcpy
#define StrLen wcslen
#define ZeroMem(p,n) memset(p,0,n)
#define EFI_IFR_REF_OP 15
typedef int EFI_BROWSER_ACTION;
typedef int EFI_QUESTION_ID;
typedef int EFI_BROWSER_ACTION_REQUEST;
typedef int BROWSER_SETTING_SCOPE;
typedef union { UINT8 b,u8;UINT16 string; } EFI_IFR_TYPE_VALUE;
typedef struct { UINT8 Type;EFI_IFR_TYPE_VALUE Value; } EFI_HII_VALUE;
typedef struct config EFI_HII_CONFIG_ACCESS_PROTOCOL;
struct config { int (*Callback)(const EFI_HII_CONFIG_ACCESS_PROTOCOL*,int,int,UINT8,EFI_IFR_TYPE_VALUE*,int*); };
typedef struct { UINT8 Enable,Ipv4Pxe,Ipv6Pxe,Ipv4Http,Ipv6Http; } NETWORK_STACK;
typedef struct { UINT8 CpuCppcType,CpuCoreEnable[12],CpuLpiState; } PLATFORM_SETUP_DATA;
typedef struct { UINT8 DisableCPPC,DisableSmallCores,EnableLPI0; } COMPLIANCE_VARSTORE_DATA;
static int gEfiNetworkStackSetupGuid,gPlatformSetupVariableGuid,gPlatformConfigFormSetGuid;
static int HiiGetBrowserData(void *a,const CHAR16 *b,size_t c,UINT8 *d) { (void)a;(void)b;(void)c;(void)d;assert(0);return 0; }
static int HiiSetBrowserData(void *a,const CHAR16 *b,size_t c,UINT8 *d,void *e) { (void)a;(void)b;(void)c;(void)d;(void)e;assert(0);return 0; }
typedef struct { LIST_ENTRY Link;UINT8 QuestionFlags;void *Expression;EFI_HII_VALUE HiiValue;UINT8 *BufferValue;UINTN StorageWidth;int QuestionId,Operand,ValueChanged;void *Storage; } FORM_BROWSER_STATEMENT;
#define FORM_BROWSER_STATEMENT_FROM_LINK(p) ((FORM_BROWSER_STATEMENT *)(p))
typedef struct { LIST_ENTRY StatementListHead; } FORM_BROWSER_FORM;
typedef struct { EFI_HII_CONFIG_ACCESS_PROTOCOL *ConfigAccess;void *HiiHandle; } FORM_BROWSER_FORMSET;
typedef struct { FORM_BROWSER_FORMSET *FormSet;FORM_BROWSER_FORM *Form; } UI_MENU_SELECTION;
static UI_MENU_SELECTION *gCurrentSelection;
static int gCallbackReconnect,gResetRequiredFormLevel,gResetRequiredSystemLevel;
#define EvaluateExpressionList(...) 0
static int mock_success(void) { return 0; }
#define SetQuestionValue(...) mock_success()
#define GetQuestionValue(...) ((void)0)
#define ValueChangedValidation(...) 0
#define IsQuestionValueChanged(...) ((void)0)
#define GetToken(...) NULL
#define SubmitForm(fs,f,level) ((void)(level))
#define DiscardForm(fs,f,level) ((void)(level))
#define FindNextMenu(selection,level) ((void)(level))
#define IsNvUpdateRequiredForFormSet(...) 0
#define PopupErrorMessage(...) mock_success()
'''
        # Helpers only used by other harnesses would otherwise be unused here.
        body = body.replace('static void NewStringCpy(CHAR16 **p,CHAR16 *q) { (void)p;(void)q;assert(0); }', '')
        body += function(source(callback_path), 'PlatformConfigCallback')
        body += function(source(SETUP + 'Presentation.c'), 'ProcessCallBackFunction')
        body += r'''
int main(void) {
 EFI_HII_CONFIG_ACCESS_PROTOCOL access={PlatformConfigCallback};
 FORM_BROWSER_FORMSET fs={.ConfigAccess=&access};FORM_BROWSER_FORM form={0};
 FORM_BROWSER_STATEMENT question={0};UI_MENU_SELECTION selection={&fs,&form};
 form.StatementListHead.ForwardLink=form.StatementListHead.BackLink=&question.Link;
 question.Link.ForwardLink=question.Link.BackLink=&form.StatementListHead;
 question.QuestionFlags=EFI_IFR_FLAG_CALLBACK;question.HiiValue.Type=0;
'''
        body += f'question.QuestionId={question};\n'
        body += r'''
 gCurrentSelection=&selection;
 EFI_STATUS open=ProcessCallBackFunction(&selection,&fs,&form,NULL,EFI_BROWSER_ACTION_FORM_OPEN,FALSE);
 EFI_STATUS retrieve=ProcessCallBackFunction(&selection,&fs,&form,NULL,EFI_BROWSER_ACTION_RETRIEVE,FALSE);
 fprintf(stderr,"StateAfterG3 id=0x%x type=%u FORM_OPEN=%d RETRIEVE=%d\n",question.QuestionId,question.HiiValue.Type,open,retrieve);
 assert(open==EFI_SUCCESS);assert(retrieve==EFI_SUCCESS);
 EFI_IFR_TYPE_VALUE value={.u8=0x5a};
 EFI_BROWSER_ACTION_REQUEST request=EFI_BROWSER_ACTION_REQUEST_EXIT;
 const int actions[]={EFI_BROWSER_ACTION_FORM_OPEN,EFI_BROWSER_ACTION_FORM_CLOSE,
                      EFI_BROWSER_ACTION_RETRIEVE,EFI_BROWSER_ACTION_CHANGED,
                      EFI_BROWSER_ACTION_CHANGING};
 for (size_t i=0;i<sizeof(actions)/sizeof(actions[0]);i++) {
  assert(PlatformConfigCallback(&access,actions[i],question.QuestionId,0,NULL,NULL)==EFI_UNSUPPORTED);
  assert(PlatformConfigCallback(&access,actions[i],question.QuestionId,4,&value,&request)==EFI_UNSUPPORTED);
  assert(PlatformConfigCallback(&access,actions[i],0xdead,4,&value,&request)==EFI_UNSUPPORTED);
  assert(value.u8==0x5a && request==EFI_BROWSER_ACTION_REQUEST_EXIT);
 }
 return 0;
}
'''
        run_c(self, body)

    def test_ui_entry_returns_normally_after_send_form_error(self):
        body = PRELUDE[:PRELUDE.index('static LIST_ENTRY *GetFirstNode')] + r'''
#define EFI_BROWSER_ACTION_REQUEST_NONE 0
#define EFI_BROWSER_ACTION_REQUEST_RESET 1
#define EFI_PROGRESS_CODE 0
#define EFI_SOFTWARE_DXE_BS_DRIVER 0
#define EFI_SW_PC_INPUT_WAIT 0
#define EFI_SW_PC_USER_SETUP 0
#define REPORT_STATUS_CODE(...) ((void)0)
typedef int EFI_BROWSER_ACTION_REQUEST;
typedef struct { int (*SetBootLogo)(void*,void*,int,int,int,int); } EFI_BOOT_LOGO_PROTOCOL;
static int gEfiBootLogoProtocolGuid,mFrontPageGuid;
static struct { void *HiiHandle; } gFrontPagePrivate;
static int form_result,entered,freed,reminded,reset;
static int locate(void *g,void *r,void *p) { (void)g;(void)r;(void)p;return EFI_NOT_FOUND; }
static struct { int (*LocateProtocol)(void*,void*,void*); } bs={locate},*gBS=&bs;
static void EfiBootManagerConnectAll(void) {}
static void EfiBootManagerRefreshAllBootOption(void) {}
static void InitializeFrontPage(void) { ++entered; }
static void FreeFrontPage(void) { ++freed; }
static void SetupResetReminder(void) { ++reminded; }
static void EnableResetRequired(void) { ++reset; }
static int send_form(void *self,void *handles,size_t count,void *guid,int form,void *screen,int *action) {
 (void)self;(void)handles;(void)guid;(void)form;(void)screen;assert(count==1);
 assert(*action==EFI_BROWSER_ACTION_REQUEST_NONE);return form_result;
}
static struct { int (*SendForm)(void*,void*,size_t,void*,int,void*,int*); } browser={send_form},*gFormBrowser2=&browser;
'''
        text = source(FRONT)
        if 'mLanguageString' in function(text, 'UiEntry'):
            body += 'static CHAR16 *mLanguageString;\n'
        body += function(text, 'CallFrontPage') + function(text, 'UiEntry') + r'''
int main(void) {
 for(int i=0;i<4;i++) {
  const int results[]={EFI_SUCCESS,EFI_NOT_FOUND,EFI_DEVICE_ERROR,EFI_BUFFER_TOO_SMALL};
  form_result=results[i];entered=freed=reminded=reset=0;
  UiEntry(TRUE);assert(entered==1 && freed==1 && reminded==1 && reset==0);
 }
 return 0;
}
'''
        run_c(self, body)


if __name__ == '__main__':
    unittest.main()
