/* SPDX-License-Identifier: GPL-2.0-only */
/* Real adapter and captured 410-edge graph; every EFI effect is simulated. */
#include "x870e_release.c"
#include "rtx4080s_graph.h"
#include "rtx4080s_captured.h"
#include "../ad104-msi/format.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

enum MODE {OK,WRONG_CODE_0,WRONG_CODE_1,WRONG_CODE_2,WRONG_CODE_3,WRONG_STOP,
 EXTRA_OWNER,SECOND_CHILD_OWNER,EXTRA_CHILD,MISSING_AMD,WRONG_AMD,
 STOP_1,STOP_2,STOP_3,STOP_4,STOP_5,LOST_AMD_EDGE,AMD_MODE_CHANGED,AMD_CONFIG_CHANGED,
 INPUT_CHANGED,OUTPUT_CHANGED,CLAIM_FAIL,ATTR_FAIL,DECODE_DRIFT,CLOSE_FAIL,
 PCI_DRIFT,POST_RELEASE_OWNER,STATUS_CLEAR,STATUS_SET,CHILD_SURVIVED,
 PARTIAL_CLAIM,REAPPEAR_CHILD,MODE_COUNT};
static enum MODE Mode;
static unsigned Checks,Stops,Enables,Closes;
static UINTN Stage;static BOOLEAN Claimed,Drift;
static EFI_BOOT_SERVICES Bs;static EFI_SYSTEM_TABLE St;
static EFI_PCI_IO_PROTOCOL Io,AmdIo;
static EFI_SIMPLE_TEXT_INPUT_PROTOCOL Keys;
static EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL Text;
static EFI_GRAPHICS_OUTPUT_PROTOCOL Gop[2];
static EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE Modes[2];
static EFI_GRAPHICS_OUTPUT_MODE_INFORMATION Infos[2];
static EFI_DRIVER_BINDING_PROTOCOL Bind[4];static EFI_LOADED_IMAGE_PROTOCOL Images[4];
static UINT8 *Code[4];static UINT32 Cfg[64],AmdCfg[64];static UINT64 Attrs;
GU_PLATFORM Gu;
#define CHECK(x) do{Checks++;if(!(x)){fprintf(stderr,"X870E FAIL case=%d line=%d: %s\n",Mode,__LINE__,#x);exit(1);}}while(0)
static EFI_HANDLE H(UINTN id){return (EFI_HANDLE)(id+1);}
static EFI_HANDLE Agent(UINTN n){static const UINTN ids[]={415,420,569,140};return H(ids[n]);}
static BOOLEAN ChildrenPresent(VOID){return Stage<5||Mode==CHILD_SURVIVED||(Drift&&Mode==REAPPEAR_CHILD);}
VOID GuLog(const char *f,...){char line[2048];va_list a;va_start(a,f);size_t n=GuFormatV(line,sizeof(line),f,a);va_end(a);CHECK(n!=(size_t)-1&&n<sizeof(line));}
VOID GuLogText(const char *s){(void)s;}
BOOLEAN GuBootLogOpen(VOID){return TRUE;}
BOOLEAN GuTargetConfig(const UINT32 *c,UINTN s,UINTN b,UINTN d,UINTN f){return c[0]==GU_BOARD_ID&&c[11]==GU_BOARD_SUBSYSTEM&&!s&&b==1&&!d&&!f;}
BOOLEAN GuAd104ColdEntry(VOID){return TRUE;}
BOOLEAN GuSafeToReturn(VOID){return TRUE;}
UINT32 GuRead32(UINT32 off){switch(off){case 0:return GU_BOARD_CHIP;case 0x110100:return 0x10;case 0x82381c:return 0x88888888;case 0x823820:return 8;case 0x823804:return 0xffffffff;default:return 0;}}
static EFI_STATUS EFIAPI KeyRead(EFI_SIMPLE_TEXT_INPUT_PROTOCOL *p,EFI_INPUT_KEY *key){return EFI_NOT_READY;}
static EFI_STATUS EFIAPI KeyReset(EFI_SIMPLE_TEXT_INPUT_PROTOCOL *p,BOOLEAN verify){return EFI_SUCCESS;}
static EFI_STATUS EFIAPI Say(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *p,CHAR16 *s){return EFI_SUCCESS;}
static EFI_STATUS EFIAPI Free(VOID *p){free(p);return EFI_SUCCESS;}
static BOOLEAN Filter(const EDGE *e) {
  if(Stage>=1&&e->handle==H(573)&&e->agent==Agent(3)&&Guid(&e->protocol,&AuxGuid))return TRUE;
  if(Stage>=2&&e->handle==H(572)&&e->agent==Agent(3)&&Guid(&e->protocol,&AuxGuid))return TRUE;
  if(Stage>=3&&e->handle==H(572)&&e->agent==Agent(0)&&Guid(&e->protocol,&TextGuid))return TRUE;
  if(Stage>=4&&e->handle==H(572)&&e->agent==Agent(1)&&Guid(&e->protocol,&GopGuid))return TRUE;
  if(Stage>=5&&e->handle==H(496)&&e->agent==Agent(2))return TRUE;
  return Stage>=1&&Mode==LOST_AMD_EDGE&&e->handle==H(574);
}
static EFI_STATUS EFIAPI Locate(EFI_LOCATE_SEARCH_TYPE kind,EFI_GUID *g,VOID *key,UINTN *count,EFI_HANDLE **h) {
  *h=NULL;
  if(kind==AllHandles) {
    *h=malloc(803*sizeof(**h));CHECK(*h);*count=0;
    for(UINTN i=0;i<803;i++){if(!ChildrenPresent()&&(i==572||i==573))continue;(*h)[(*count)++]=H(i);}
    return EFI_SUCCESS;
  }
  CHECK(kind==ByProtocol&&Guid(g,&GopGuid));
  *h=malloc(2*sizeof(**h));CHECK(*h);*count=0;
  if(ChildrenPresent())(*h)[(*count)++]=H(572);
  if(Mode!=MISSING_AMD)(*h)[(*count)++]=H(574);
  return *count?EFI_SUCCESS:EFI_NOT_FOUND;
}
static EFI_STATUS EFIAPI Handle(EFI_HANDLE h,EFI_GUID *g,VOID **p) {
  *p=NULL;
  if(Guid(g,&PathGuid)) {
    if(h==H(496))*p=(VOID *)PciPath;
    if(h==H(540))*p=(VOID *)AmdPciPath;
    if(h==H(574))*p=(VOID *)AmdGopPath;
    if(ChildrenPresent()&&h==H(572))*p=(VOID *)ChildPaths[0];
    if(ChildrenPresent()&&h==H(573))*p=(VOID *)ChildPaths[1];
    if(Mode==EXTRA_CHILD&&h==H(801)){static UINT8 extra[36];memcpy(extra,ChildPaths[1],36);extra[28]=0x90;*p=extra;}
  }
  if(Guid(g,&PciGuid)){if(h==H(496))*p=&Io;if(h==H(540))*p=&AmdIo;}
  if(Guid(g,&GopGuid)){if(ChildrenPresent()&&h==H(572))*p=&Gop[0];if(h==H(574))*p=&Gop[1];}
  if(h==H(418)){if(Guid(g,&InputGuid))*p=&Keys;if(Guid(g,&TextGuid))*p=&Text;}
  for(UINTN i=0;i<4;i++){if(Guid(g,&BindingGuid)&&h==Agent(i))*p=&Bind[i];if(Guid(g,&ImageGuid)&&h==Bind[i].ImageHandle)*p=&Images[i];}
  return *p?EFI_SUCCESS:EFI_UNSUPPORTED;
}
static EFI_STATUS EFIAPI Protocols(EFI_HANDLE h,EFI_GUID ***out,UINTN *n) {
  EFI_GUID *unique[64];*n=0;
  for(UINTN i=0;i<ARRAY_SIZE(CapturedEdges);i++)if(CapturedEdges[i].handle==h&&!Filter(&CapturedEdges[i])) {
    BOOLEAN seen=FALSE;for(UINTN j=0;j<*n;j++)if(Guid(unique[j],&CapturedEdges[i].protocol))seen=TRUE;
    if(!seen)unique[(*n)++]=(EFI_GUID *)&CapturedEdges[i].protocol;
  }
  if(h==H(496)){BOOLEAN seen=FALSE;for(UINTN j=0;j<*n;j++)if(Guid(unique[j],&PciGuid))seen=TRUE;if(!seen)unique[(*n)++]=&PciGuid;}
  if(!*n)unique[(*n)++]=&PathGuid;
  *out=malloc(*n*sizeof(**out));CHECK(*out);memcpy(*out,unique,*n*sizeof(**out));return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Opens(EFI_HANDLE h,EFI_GUID *g,EFI_OPEN_PROTOCOL_INFORMATION_ENTRY **out,UINTN *n) {
  *out=calloc(128,sizeof(**out));CHECK(*out);*n=0;
  for(UINTN i=0;i<ARRAY_SIZE(CapturedEdges);i++) {
    const EDGE *e=&CapturedEdges[i];if(e->handle!=h||!Guid(g,&e->protocol)||Filter(e))continue;
    (*out)[(*n)++]=(EFI_OPEN_PROTOCOL_INFORMATION_ENTRY){e->agent,e->controller,e->attributes,e->count};
  }
  if(Claimed&&h==H(496)&&Guid(g,&PciGuid))(*out)[(*n)++]=(EFI_OPEN_PROTOCOL_INFORMATION_ENTRY){H(800),NULL,EFI_OPEN_PROTOCOL_EXCLUSIVE,1};
  if((Mode==EXTRA_OWNER||(Drift&&Mode==POST_RELEASE_OWNER))&&h==H(496)&&Guid(g,&PciGuid))(*out)[(*n)++]=(EFI_OPEN_PROTOCOL_INFORMATION_ENTRY){H(802),H(496),16,1};
  if(Mode==SECOND_CHILD_OWNER&&h==H(573)&&Guid(g,&AuxGuid))(*out)[(*n)++]=(EFI_OPEN_PROTOCOL_INFORMATION_ENTRY){H(802),H(573),16,1};
  CHECK(*n<=128);return EFI_SUCCESS;
}
static EFI_TPL EFIAPI Raise(EFI_TPL t){CHECK(t==TPL_NOTIFY);return TPL_APPLICATION;}
static VOID EFIAPI Restore(EFI_TPL t){CHECK(t==TPL_APPLICATION);}
static EFI_STATUS EFIAPI Location(EFI_PCI_IO_PROTOCOL *p,UINTN *s,UINTN *b,UINTN *d,UINTN *f){CHECK(p==&Io||p==&AmdIo);*s=*d=*f=0;*b=p==&Io?1:0x7a;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI ConfigRead(EFI_PCI_IO_PROTOCOL *p,EFI_PCI_IO_PROTOCOL_WIDTH w,UINT32 off,UINTN count,VOID *out){CHECK((p==&Io||p==&AmdIo)&&w==EfiPciIoWidthUint32&&!off&&count==64);memcpy(out,p==&Io?Cfg:AmdCfg,sizeof(Cfg));return EFI_SUCCESS;}
static EFI_STATUS EFIAPI GetBar(EFI_PCI_IO_PROTOCOL *p,UINT8 index,UINT64 *support,VOID **out){
  CHECK(p==&Io&&!index);*support=0;EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *d=calloc(1,sizeof(*d));CHECK(d);
  d->Desc=0x8a;d->Len=43;d->AddrRangeMin=GU_BAR0_BASE;d->AddrRangeMax=GU_BAR0_BASE+0xffffff;d->AddrLen=0x1000000;*out=d;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Attributes(EFI_PCI_IO_PROTOCOL *p,EFI_PCI_IO_PROTOCOL_ATTRIBUTE_OPERATION op,UINT64 mask,UINT64 *out){
  CHECK(p==&Io);
  if(op==EfiPciIoAttributeOperationGet){CHECK(out&&!mask);*out=Attrs;return EFI_SUCCESS;}
  if(op==EfiPciIoAttributeOperationSupported){CHECK(out&&!mask);*out=EFI_PCI_IO_ATTRIBUTE_IO|EFI_PCI_IO_ATTRIBUTE_MEMORY;return EFI_SUCCESS;}
  CHECK(op==EfiPciIoAttributeOperationEnable&&mask==(EFI_PCI_IO_ATTRIBUTE_IO|EFI_PCI_IO_ATTRIBUTE_MEMORY)&&!out&&Claimed);Enables++;
  if(Mode==ATTR_FAIL)return EFI_DEVICE_ERROR;Attrs|=mask;Cfg[1]|=3;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Open(EFI_HANDLE h,EFI_GUID *g,VOID **out,EFI_HANDLE a,EFI_HANDLE c,UINT32 attrs){
  CHECK(h==H(496)&&Guid(g,&PciGuid)&&a==H(800)&&!c&&attrs==EFI_OPEN_PROTOCOL_EXCLUSIVE&&Stage==5&&!Claimed);
  if(Mode==CLAIM_FAIL)return EFI_ACCESS_DENIED;Claimed=TRUE;*out=Mode==PARTIAL_CLAIM?&AmdIo:&Io;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Close(EFI_HANDLE h,EFI_GUID *g,EFI_HANDLE a,EFI_HANDLE c){
  CHECK(h==H(496)&&Guid(g,&PciGuid)&&a==H(800)&&!c&&Claimed);Closes++;
  if(Mode==CLOSE_FAIL)return EFI_DEVICE_ERROR;Claimed=FALSE;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Disconnect(EFI_HANDLE controller,EFI_HANDLE driver,EFI_HANDLE child){
  static const UINTN controllers[]={573,572,572,572,496},agents[]={3,3,0,1,2};
  CHECK(!child&&Stops<5&&controller==H(controllers[Stops])&&driver==Agent(agents[Stops]));Stops++;Stage=Stops;
  if(Stage==5){Cfg[1]&=~3u;Attrs=0;if(Mode==DECODE_DRIFT)Cfg[2]^=1;}
  if(Stage==1){
    if(Mode==INPUT_CHANGED)St.ConIn=NULL;
    if(Mode==OUTPUT_CHANGED)Text.OutputString=NULL;
    if(Mode==AMD_MODE_CHANGED)Infos[1].HorizontalResolution++;
    if(Mode==AMD_CONFIG_CHANGED)AmdCfg[4]^=0x100000;
  }
  if(Mode>=STOP_1&&Mode<=STOP_5&&Stage==(UINTN)(Mode-STOP_1+1))return EFI_DEVICE_ERROR;
  return EFI_SUCCESS;
}
static void Setup(void) {
  memset(&Gu,0,sizeof(Gu));memset(&Plan,0,sizeof(Plan));memset(&Bs,0,sizeof(Bs));memset(&St,0,sizeof(St));memset(&Io,0,sizeof(Io));memset(&AmdIo,0,sizeof(AmdIo));
  memset(&Keys,0,sizeof(Keys));memset(&Text,0,sizeof(Text));memcpy(Cfg,CapturedConfig,sizeof(Cfg));memset(AmdCfg,0,sizeof(AmdCfg));
  AmdCfg[0]=Mode==WRONG_AMD?0x164d1002:0x164e1002;AmdCfg[2]=0x03000000;
  Stage=Stops=Enables=Closes=0;Claimed=Drift=FALSE;
  Bs.LocateHandleBuffer=Locate;Bs.HandleProtocol=Handle;Bs.ProtocolsPerHandle=Protocols;Bs.OpenProtocolInformation=Opens;Bs.FreePool=Free;
  Bs.RaiseTPL=Raise;Bs.RestoreTPL=Restore;Bs.OpenProtocol=Open;Bs.CloseProtocol=Close;Bs.DisconnectController=Disconnect;
  St.BootServices=&Bs;St.ConsoleInHandle=St.ConsoleOutHandle=St.StandardErrorHandle=H(418);St.ConIn=&Keys;St.ConOut=St.StdErr=&Text;
  Keys.ReadKeyStroke=KeyRead;Keys.Reset=KeyReset;Keys.WaitForKey=(EFI_EVENT)1;Text.OutputString=Say;
  for(UINTN i=0;i<2;i++){memset(&Modes[i],0,sizeof(Modes[i]));memset(&Infos[i],0,sizeof(Infos[i]));Gop[i].Mode=&Modes[i];Modes[i].Info=&Infos[i];Modes[i].SizeOfInfo=sizeof(Infos[i]);Modes[i].FrameBufferBase=i?0xfc10000000ULL:0xf800000000ULL;Modes[i].FrameBufferSize=0x258000;}
  Io.Pci.Read=AmdIo.Pci.Read=ConfigRead;Io.GetLocation=AmdIo.GetLocation=Location;Io.GetBarAttributes=GetBar;Io.Attributes=Attributes;
  Attrs=EFI_PCI_IO_ATTRIBUTE_IO|EFI_PCI_IO_ATTRIBUTE_MEMORY;
  Gu.bs=&Bs;Gu.pci=&Io;Gu.image=H(800);Gu.handle=H(496);Gu.profile_verified=Gu.prom_verified=TRUE;Gu.pcie_cap=0x78;
  for(UINTN i=0;i<4;i++) {
    const DRIVER *d=&Drivers[i];memset(&Bind[i],0,sizeof(Bind[i]));memset(&Images[i],0,sizeof(Images[i]));
    Bind[i].DriverBindingHandle=Agent(i);Bind[i].ImageHandle=i<2?H(i?419:414):Agent(i);Bind[i].Version=d->version;
    Bind[i].Stop=(EFI_DRIVER_BINDING_STOP)(UINTN)(Code[i]+d->stop);Images[i].ImageBase=Code[i];Images[i].ImageSize=d->size;Images[i].ImageCodeType=d->type;
    Images[i].DeviceHandle=i==2?H(496):H(7);Images[i].FilePath=(VOID *)d->file;
  }
  if(Mode==WRONG_STOP)Bind[3].Stop=(EFI_DRIVER_BINDING_STOP)((UINTN)Bind[3].Stop+1);
}
int main(int argc,char **argv) {
  CHECK(argc==2);static const UINTN labels[]={415,420,569,140};
  for(UINTN i=0;i<4;i++) {
    char name[4096];snprintf(name,sizeof(name),"%s/driver-%u.bin",argv[1],(unsigned)labels[i]);FILE *f=fopen(name,"rb");CHECK(f);
    Code[i]=calloc(1,Drivers[i].size);CHECK(Code[i]);CHECK(fread(Code[i]+Drivers[i].rva,1,Drivers[i].bytes,f)==Drivers[i].bytes&&fgetc(f)==EOF);fclose(f);
  }
  for(Mode=OK;Mode<MODE_COUNT;Mode++) {
    Setup();int corrupt=Mode>=WRONG_CODE_0&&Mode<=WRONG_CODE_3?(int)Mode-WRONG_CODE_0:-1;
    if(corrupt>=0)Code[corrupt][Drivers[corrupt].rva]^=1;
    EFI_STATUS s=GuMsiPreflight(&St,H(800),H(496));
    if(corrupt>=0)Code[corrupt][Drivers[corrupt].rva]^=1;
    if(Mode>=WRONG_CODE_0&&Mode<=WRONG_AMD){CHECK(s!=EFI_SUCCESS&&!Stops&&!GuMsiMutated());continue;}
    CHECK(s==EFI_SUCCESS);CHECK(Original.count==410);s=GuMsiRelease();
    if((Mode>=STOP_1&&Mode<=DECODE_DRIFT)||Mode==CHILD_SURVIVED||Mode==PARTIAL_CLAIM){CHECK(s!=EFI_SUCCESS&&GuMsiMutated());CHECK(GuMsiRelease()!=EFI_SUCCESS);continue;}
    CHECK(s==EFI_SUCCESS&&GuMsiReleased()&&Stops==5&&Enables==1);CHECK(GuMsiRelease()!=EFI_SUCCESS&&Stops==5);
    Gu.quiesced=TRUE;Gu.flrs=1;Drift=TRUE;
    if(Mode==PCI_DRIFT)Cfg[4]^=0x100000;
    if(Mode==STATUS_CLEAR)Cfg[0x80/4]&=~0x90000u;
    if(Mode==STATUS_SET)Cfg[0x80/4]|=0x20000u;
    if(Mode==PCI_DRIFT||Mode==POST_RELEASE_OWNER||Mode==STATUS_SET||Mode==REAPPEAR_CHILD){CHECK(!GuMsiReleased()&&GuMsiFinish()!=EFI_SUCCESS&&!Closes);continue;}
    s=GuMsiFinish();
    if(Mode==CLOSE_FAIL){CHECK(s!=EFI_SUCCESS&&Claimed&&!GuMsiReadyForWindows());continue;}
    CHECK(s==EFI_SUCCESS&&!Claimed&&Closes==1&&GuMsiReadyForWindows());
  }
  for(UINTN i=0;i<4;i++)free(Code[i]);
  printf("X870E ownership: %u scenarios, %u checks; captured graph/code, all effects simulated.\n",MODE_COUNT,Checks);return 0;
}
