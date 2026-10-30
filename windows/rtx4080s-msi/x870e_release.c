/* SPDX-License-Identifier: GPL-2.0-only */
/* MSI X870E / AD103 ownership adapter. Derived from Latence.app's AD104
 * snapshot/delta adapter; original notices remain in ../ad104-msi.
 * Only five exact controller/driver disconnects are admitted. AMD display,
 * console/input and all unrelated relationships must survive unchanged.
 * No guessed driver globals, reconnect, retry or firmware-variable writes. */
#include "../ad104-msi/msi_release.h"
#include "../ad104-msi/boot_io.h"
#include "../ad104-msi/profile.h"
#include "../ad104-msi/sha256.h"
#include "../ad104-msi/compat/compat.h"
#include <Protocol/GraphicsOutput.h>
#include <Protocol/DriverBinding.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/SimpleTextIn.h>
#include <Protocol/SimpleTextOut.h>
#include <IndustryStandard/Acpi10.h>

#define MAX_EDGES 4096
typedef struct {EFI_HANDLE handle,agent,controller;EFI_GUID protocol;UINT32 attributes,count;} EDGE;
typedef struct {UINTN count;EDGE edges[MAX_EDGES];} GRAPH;
typedef struct {
  EFI_SYSTEM_TABLE *table;
  EFI_HANDLE self,pci,child[2],amd,amd_pci,agent[4],console,input,error;
  EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *text,*err;
  EFI_SIMPLE_TEXT_INPUT_PROTOCOL *keys;
  EFI_SIMPLE_TEXT_INPUT_PROTOCOL keys_value;
  EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL text_value,err_value;
  UINTN key_reset,key_read,text_output,err_output;EFI_EVENT key_wait;
  EFI_GRAPHICS_OUTPUT_PROTOCOL *gop[2];
  EFI_GRAPHICS_OUTPUT_PROTOCOL gop_value[2];
  EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE mode[2];
  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION info[2];
  EFI_PCI_IO_PROTOCOL *amd_io;UINT32 amd_config[64],config[64];UINT64 attributes;
  UINTN stage;BOOLEAN ready,mutated,claimed;
} PLAN;
static PLAN Plan;
static GRAPH Original,Current;
static EFI_GUID PciGuid=EFI_PCI_IO_PROTOCOL_GUID,PathGuid=EFI_DEVICE_PATH_PROTOCOL_GUID;
static EFI_GUID GopGuid=EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID,TextGuid=EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL_GUID;
static EFI_GUID InputGuid=EFI_SIMPLE_TEXT_INPUT_PROTOCOL_GUID;
static EFI_GUID BindingGuid=EFI_DRIVER_BINDING_PROTOCOL_GUID,ImageGuid=EFI_LOADED_IMAGE_PROTOCOL_GUID;
/* Captured firmware protocol 1c0c34f6-d380-41fa-a049-8ad06c1a66aa.
 * Its name is not assumed: match owner, loaded code, controller and edge. */
static EFI_GUID AuxGuid={0x1c0c34f6,0xd380,0x41fa,{0xa0,0x49,0x8a,0xd0,0x6c,0x1a,0x66,0xaa}};
static const UINT8 PciPath[]={2,1,12,0,0xd0,0x41,3,10,0,0,0,0,1,1,6,0,1,1,1,1,6,0,0,0,127,255,4,0};
static const UINT8 ChildPaths[2][36]={
 {2,1,12,0,0xd0,0x41,3,10,0,0,0,0,1,1,6,0,1,1,1,1,6,0,0,0,2,3,8,0,0x30,3,1,0x80,127,255,4,0},
 {2,1,12,0,0xd0,0x41,3,10,0,0,0,0,1,1,6,0,1,1,1,1,6,0,0,0,2,3,8,0,0x70,3,1,0x80,127,255,4,0}};
static const UINT8 AmdPciPath[]={2,1,12,0,0xd0,0x41,3,10,0,0,0,0,1,1,6,0,1,8,1,1,6,0,0,0,127,255,4,0};
static const UINT8 AmdGopPath[]={2,1,12,0,0xd0,0x41,3,10,0,0,0,0,1,1,6,0,1,8,1,1,6,0,0,0,2,3,8,0,3,3,1,0x80,127,255,4,0};
static const UINT8 OutFile[]={4,6,20,0,0x7d,0x49,0x8a,0x62,0xf6,0x2b,0x64,0x42,0x87,0x41,6,0x9d,0xbd,0x33,0x99,0xd6,127,255,4,0};
static const UINT8 GfxFile[]={4,6,20,0,0xdd,0xab,0xe7,0x43,0x52,0xe3,0xfb,0x4c,0xa2,0x30,0x4c,0xdc,0x1d,0x35,0x0e,0x5c,127,255,4,0};
static const UINT8 NvFile[]={4,4,28,0,'n',0,'v',0,'g',0,'o',0,'p',0,'-',0,'a',0,'d',0,'1',0,'x',0,'x',0,0,0,127,255,4,0};
static const UINT8 AuxFile[]={4,6,20,0,0x0c,3,0xa7,0xc7,0xd8,0xc3,0xee,0x45,0xbe,0xd9,0x5d,0x9e,0x76,0x76,0x29,0x53,127,255,4,0};
typedef struct {UINTN size,stop,rva,bytes;UINT32 version;EFI_MEMORY_TYPE type;const UINT8 *file;UINTN file_bytes;const char *hash;} DRIVER;
static const DRIVER Drivers[]={
 {45056,0x2390,0x1000,23604,0x10,EfiBootServicesCode,OutFile,sizeof(OutFile),"a90858f4bc964ce4b089799a6e123cdcc8724fbcab4e487014f5647660b19c89"},
 {36864,0x1c8c,0x1000,10176,0x10,EfiBootServicesCode,GfxFile,sizeof(GfxFile),"9662260e9254673c30a0ea471e49726da5cfb918bb3316d0eaa22e29d038d05d"},
 {162496,0x6ec,0x280,138183,0x70011,EfiBootServicesCode,NvFile,sizeof(NvFile),"216c7270fa08ad9e4c3694d34d825e9e904554a1af5ffa2aeead4317c5537b37"},
 {36864,0x1504,0x1000,14770,1,EfiRuntimeServicesCode,AuxFile,sizeof(AuxFile),"2344e72b0afcbc164b3ce3160772f4250378998bb7f8fd000a650aa0b1a80afe"}};
#define NOTE(...) do{if(GuBootLogOpen())GuLog(__VA_ARGS__);}while(0)
static BOOLEAN Equal(const VOID *a,const VOID *b,UINTN n){return !memcmp(a,b,n);}
static BOOLEAN Guid(const EFI_GUID *a,const EFI_GUID *b){return Equal(a,b,16);}
static EFI_STATUS Interface(EFI_HANDLE h,EFI_GUID *g,VOID **p) {
  *p=NULL;EFI_STATUS s=Plan.table->BootServices->HandleProtocol(h,g,p);
  return s==EFI_SUCCESS&&!*p?EFI_COMPROMISED_DATA:s;
}
static BOOLEAN Path(EFI_HANDLE h,const UINT8 *bytes,UINTN count) {
  EFI_DEVICE_PATH_PROTOCOL *p=NULL;
  if(Interface(h,&PathGuid,(VOID **)&p)!=EFI_SUCCESS)return FALSE;
  UINTN at=0;
  while(at+4<=count) {
    UINTN n=p->Length[0]+((UINTN)p->Length[1]<<8);
    if(n<4||n>count-at||!Equal(p,bytes+at,n))return FALSE;
    if(p->Type==END_DEVICE_PATH_TYPE)return n==4&&p->SubType==END_ENTIRE_DEVICE_PATH_SUBTYPE&&at+4==count;
    at+=n;p=(VOID *)((UINT8 *)p+n);
  }
  return FALSE;
}
static EFI_STATUS Snapshot(GRAPH *g,BOOLEAN discover) {
  EFI_BOOT_SERVICES *b=Plan.table->BootServices;EFI_HANDLE *hs=NULL;UINTN count=0;g->count=0;
  EFI_STATUS s=b->LocateHandleBuffer(AllHandles,NULL,NULL,&count,&hs);
  if(s==EFI_SUCCESS&&(!hs||!count||count>4096))s=EFI_BAD_BUFFER_SIZE;
  UINTN targets=0,children[2]={0},amd=0;
  for(UINTN i=0;s==EFI_SUCCESS&&i<count;i++) {
    /* No extra GPU child may hide behind a path without active opens. */
    EFI_DEVICE_PATH_PROTOCOL *dp=NULL;
    if(Interface(hs[i],&PathGuid,(VOID **)&dp)==EFI_SUCCESS) {
      const UINT8 *p=(const UINT8 *)dp;UINTN at=0;BOOLEAN prefix=TRUE;
      while(at<sizeof(PciPath)-4) {
        UINTN n=p[at+2]+((UINTN)p[at+3]<<8);
        if(n<4||n>sizeof(PciPath)-4-at||!Equal(p+at,PciPath+at,n)){prefix=FALSE;break;}
        at+=n;
      }
      if(prefix&&!Path(hs[i],PciPath,sizeof(PciPath))&&!Path(hs[i],ChildPaths[0],sizeof(ChildPaths[0]))&&!Path(hs[i],ChildPaths[1],sizeof(ChildPaths[1]))){s=EFI_UNSUPPORTED;break;}
    }
    if(Path(hs[i],PciPath,sizeof(PciPath))){if(hs[i]!=Plan.pci){s=EFI_UNSUPPORTED;break;}targets++;}
    for(UINTN k=0;k<2;k++)if(Path(hs[i],ChildPaths[k],sizeof(ChildPaths[k]))) {
      children[k]++;if(discover)Plan.child[k]=hs[i];else if(hs[i]!=Plan.child[k])s=EFI_NOT_READY;
    }
    if(Path(hs[i],AmdPciPath,sizeof(AmdPciPath))){amd++;if(discover)Plan.amd_pci=hs[i];else if(hs[i]!=Plan.amd_pci)s=EFI_NOT_READY;}
    EFI_GUID **ps=NULL;UINTN pc=0;
    if(s==EFI_SUCCESS)s=b->ProtocolsPerHandle(hs[i],&ps,&pc);
    if(s==EFI_SUCCESS&&(!ps||!pc||pc>64))s=EFI_BAD_BUFFER_SIZE;
    for(UINTN j=0;s==EFI_SUCCESS&&j<pc;j++) {
      EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *es=NULL;UINTN ec=0;
      if(!ps[j]){s=EFI_COMPROMISED_DATA;break;}
      s=b->OpenProtocolInformation(hs[i],ps[j],&es,&ec);
      if(s==EFI_SUCCESS&&(ec>128||(ec&&!es)))s=EFI_BAD_BUFFER_SIZE;
      for(UINTN k=0;s==EFI_SUCCESS&&k<ec;k++) {
        EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *e=&es[k];
        if(e->Attributes==1||e->Attributes==2||e->Attributes==4)continue;
        BOOLEAN own=Plan.claimed&&hs[i]==Plan.pci&&e->AgentHandle==Plan.self&&!e->ControllerHandle&&e->Attributes==EFI_OPEN_PROTOCOL_EXCLUSIVE&&e->OpenCount==1;
        if(g->count==MAX_EDGES||!e->AgentHandle||(!e->ControllerHandle&&!own)||!e->OpenCount){s=EFI_COMPROMISED_DATA;break;}
        g->edges[g->count++]=(EDGE){hs[i],e->AgentHandle,e->ControllerHandle,*ps[j],e->Attributes,e->OpenCount};
      }
      if(es&&b->FreePool(es)!=EFI_SUCCESS)s=EFI_DEVICE_ERROR;
    }
    if(ps&&b->FreePool(ps)!=EFI_SUCCESS)s=EFI_DEVICE_ERROR;
  }
  if(hs&&b->FreePool(hs)!=EFI_SUCCESS)s=EFI_DEVICE_ERROR;
  if(s==EFI_SUCCESS&&(targets!=1||amd!=1||children[0]!=(Plan.stage<5?1u:0u)||children[1]!=(Plan.stage<5?1u:0u)))s=EFI_NOT_READY;
  return s;
}
static BOOLEAN SameEdge(const EDGE *a,const EDGE *b) {
  return a->handle==b->handle&&a->agent==b->agent&&a->controller==b->controller&&a->attributes==b->attributes&&a->count==b->count&&Guid(&a->protocol,&b->protocol);
}
static BOOLEAN Removed(const EDGE *e,UINTN stage) {
  if(stage>=1&&e->handle==Plan.child[1]&&e->agent==Plan.agent[3]&&Guid(&e->protocol,&AuxGuid))return TRUE;
  if(stage>=2&&e->handle==Plan.child[0]&&e->agent==Plan.agent[3]&&Guid(&e->protocol,&AuxGuid))return TRUE;
  if(stage>=3&&e->handle==Plan.child[0]&&e->agent==Plan.agent[0]&&Guid(&e->protocol,&TextGuid))return TRUE;
  if(stage>=4&&e->handle==Plan.child[0]&&e->agent==Plan.agent[1]&&Guid(&e->protocol,&GopGuid))return TRUE;
  return stage>=5&&e->handle==Plan.pci&&e->agent==Plan.agent[2];
}
static BOOLEAN OwnEdge(const EDGE *e) {
  return e->handle==Plan.pci&&e->agent==Plan.self&&!e->controller&&Guid(&e->protocol,&PciGuid)&&e->attributes==EFI_OPEN_PROTOCOL_EXCLUSIVE&&e->count==1;
}
static BOOLEAN Expected(UINTN stage,BOOLEAN claimed) {
  UINTN own=0,expected=0;
  for(UINTN i=0;i<Original.count;i++) {
    if(Removed(&Original.edges[i],stage))continue;
    expected++;UINTN matches=0;
    for(UINTN j=0;j<Current.count;j++)if(SameEdge(&Original.edges[i],&Current.edges[j]))matches++;
    if(matches!=1)return FALSE;
  }
  for(UINTN j=0;j<Current.count;j++)if(OwnEdge(&Current.edges[j]))own++;
  return own==(claimed?1u:0u)&&Current.count==expected+own;
}
static EFI_STATUS Driver(UINTN which) {
  const DRIVER *pin=&Drivers[which];EFI_HANDLE a=Plan.agent[which];
  EFI_DRIVER_BINDING_PROTOCOL *d=NULL;EFI_LOADED_IMAGE_PROTOCOL *im=NULL;
  EFI_STATUS s=Interface(a,&BindingGuid,(VOID **)&d);
  if(s!=EFI_SUCCESS||!d->Stop||d->DriverBindingHandle!=a||!d->ImageHandle||d->Version!=pin->version)return EFI_UNSUPPORTED;
  s=Interface(d->ImageHandle,&ImageGuid,(VOID **)&im);
  if(s!=EFI_SUCCESS||!im->ImageBase||im->ImageSize!=pin->size||im->ImageCodeType!=pin->type||
     (UINTN)d->Stop!=(UINTN)im->ImageBase+pin->stop||!im->FilePath||!Equal(im->FilePath,pin->file,pin->file_bytes)||
     (which==2&&(im->DeviceHandle!=Plan.pci||d->ImageHandle!=a)))return EFI_UNSUPPORTED;
  return GuHashMatches((UINT8 *)im->ImageBase+pin->rva,pin->bytes,pin->hash)?EFI_SUCCESS:EFI_CRC_ERROR;
}
static EFI_STATUS Alive(VOID) {
  EFI_SYSTEM_TABLE *t=Plan.table;VOID *in=NULL,*out=NULL,*err=NULL;
  if(t->ConsoleOutHandle!=Plan.console||t->ConsoleInHandle!=Plan.input||t->StandardErrorHandle!=Plan.error||
     t->ConIn!=Plan.keys||t->ConOut!=Plan.text||t->StdErr!=Plan.err||!t->ConIn||
     (UINTN)t->ConIn->Reset!=Plan.key_reset||(UINTN)t->ConIn->ReadKeyStroke!=Plan.key_read||t->ConIn->WaitForKey!=Plan.key_wait||
     (UINTN)t->ConOut->OutputString!=Plan.text_output||(UINTN)t->StdErr->OutputString!=Plan.err_output)return EFI_NOT_READY;
  if(!Equal(t->ConIn,&Plan.keys_value,sizeof(Plan.keys_value))||!Equal(t->ConOut,&Plan.text_value,sizeof(Plan.text_value))||!Equal(t->StdErr,&Plan.err_value,sizeof(Plan.err_value)))return EFI_NOT_READY;
  EFI_STATUS s=Interface(Plan.input,&InputGuid,&in);
  if(s==EFI_SUCCESS)s=Interface(Plan.console,&TextGuid,&out);
  if(s==EFI_SUCCESS)s=Interface(Plan.error,&TextGuid,&err);
  return s==EFI_SUCCESS&&(in!=Plan.keys||out!=Plan.text||err!=Plan.err)?EFI_NOT_READY:s;
}
static EFI_STATUS Gops(BOOLEAN first) {
  EFI_BOOT_SERVICES *b=Plan.table->BootServices;EFI_HANDLE *hs=NULL;UINTN count=0,seen[2]={0};
  EFI_STATUS s=b->LocateHandleBuffer(ByProtocol,&GopGuid,NULL,&count,&hs);
  if(s==EFI_SUCCESS&&(!hs||count!=(Plan.stage<5?2u:1u)))s=EFI_UNSUPPORTED;
  for(UINTN i=0;s==EFI_SUCCESS&&i<count;i++) {
    UINTN k=Path(hs[i],AmdGopPath,sizeof(AmdGopPath))?1:0;
    if(!k&&(Plan.stage>=5||hs[i]!=Plan.child[0]||!Path(hs[i],ChildPaths[0],sizeof(ChildPaths[0])))){s=EFI_UNSUPPORTED;break;}
    EFI_GRAPHICS_OUTPUT_PROTOCOL *g=NULL;s=Interface(hs[i],&GopGuid,(VOID **)&g);
    if(s!=EFI_SUCCESS||++seen[k]!=1||!g->Mode||!g->Mode->Info||g->Mode->SizeOfInfo!=sizeof(*g->Mode->Info)||!g->Mode->FrameBufferBase||!g->Mode->FrameBufferSize){s=EFI_UNSUPPORTED;break;}
    if(first){Plan.gop[k]=g;Plan.gop_value[k]=*g;Plan.mode[k]=*g->Mode;Plan.info[k]=*g->Mode->Info;if(k)Plan.amd=hs[i];}
    else if((k&&hs[i]!=Plan.amd)||g!=Plan.gop[k]||!Equal(g,&Plan.gop_value[k],sizeof(Plan.gop_value[k]))||!Equal(g->Mode,&Plan.mode[k],sizeof(Plan.mode[k]))||!Equal(g->Mode->Info,&Plan.info[k],sizeof(Plan.info[k])))s=EFI_NOT_READY;
  }
  if(hs&&b->FreePool(hs)!=EFI_SUCCESS)s=EFI_DEVICE_ERROR;
  if(s==EFI_SUCCESS&&(seen[1]!=1||seen[0]!=(Plan.stage<5?1u:0u)))s=EFI_NOT_READY;
  EFI_PCI_IO_PROTOCOL *io=NULL;UINT32 c[64];UINTN seg,bus,dev,fun;
  if(s==EFI_SUCCESS)s=Interface(Plan.amd_pci,&PciGuid,(VOID **)&io);
  if(s!=EFI_SUCCESS)return s;
  if(!io->Pci.Read||!io->GetLocation)return EFI_UNSUPPORTED;
  s=io->GetLocation(io,&seg,&bus,&dev,&fun);
  if(s==EFI_SUCCESS)s=io->Pci.Read(io,EfiPciIoWidthUint32,0,64,c);
  if(s!=EFI_SUCCESS||seg||bus!=0x7a||dev||fun||c[0]!=0x164e1002||c[2]>>24!=3)return EFI_UNSUPPORTED;
  if(first){Plan.amd_io=io;memcpy(Plan.amd_config,c,sizeof(c));}
  else if(io!=Plan.amd_io||!Equal(c,Plan.amd_config,sizeof(c)))return EFI_NOT_READY;
  return EFI_SUCCESS;
}
static EFI_STATUS ReadConfig(UINT32 c[64]) {
  EFI_PCI_IO_PROTOCOL *io=NULL;UINTN s,b,d,f;
  if(Interface(Plan.pci,&PciGuid,(VOID **)&io)!=EFI_SUCCESS||io!=Gu.pci||!io->GetLocation||!io->Pci.Read)return EFI_NOT_READY;
  EFI_STATUS status=io->GetLocation(io,&s,&b,&d,&f);
  if(status==EFI_SUCCESS)status=io->Pci.Read(io,EfiPciIoWidthUint32,0,64,c);
  if(status==EFI_SUCCESS&&(!GuTargetConfig(c,s,b,d,f)||(c[1]&4)))status=EFI_UNSUPPORTED;
  return status;
}
static BOOLEAN ConfigStable(const UINT32 c[64]) {
  for(UINTN i=0;i<64;i++) {
    UINT32 drift=c[i]^Plan.config[i];
    if(!drift)continue;
    /* Only pre-existing PCIe corrected/unsupported-request status may clear. */
    if(Gu.flrs!=1||i!=(Gu.pcie_cap+8u)/4u||(c[i]&~Plan.config[i])||(drift&~0x00090000u))return FALSE;
  }
  return TRUE;
}
static EFI_STATUS Bar(VOID) {
  EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *d=NULL;UINT64 support=0;
  if(!Gu.pci->GetBarAttributes)return EFI_UNSUPPORTED;
  EFI_STATUS s=Gu.pci->GetBarAttributes(Gu.pci,0,&support,(VOID **)&d);
  if(s==EFI_SUCCESS&&(!d||d->Desc!=0x8a||d->Len!=43||d->ResType||d->AddrRangeMin!=GU_BAR0_BASE||
     d->AddrRangeMax!=GU_BAR0_BASE+0xffffff||d->AddrTranslationOffset||d->AddrLen!=0x1000000))s=EFI_UNSUPPORTED;
  if(d&&Plan.table->BootServices->FreePool(d)!=EFI_SUCCESS)s=EFI_DEVICE_ERROR;
  return s;
}
static EFI_STATUS Consumers(VOID) {
  UINTN counts[7]={0};
  for(UINTN i=0;i<Original.count;i++) {
    const EDGE *e=&Original.edges[i];int slot=-1,agent=-1;
    if(e->handle==Plan.pci) {
      if(Guid(&e->protocol,&PciGuid)&&e->attributes==16&&e->controller==Plan.pci){slot=0;agent=2;}
      else if(Guid(&e->protocol,&PathGuid)&&e->attributes==8&&e->controller==Plan.child[0]){slot=1;agent=2;}
      else if(Guid(&e->protocol,&PathGuid)&&e->attributes==8&&e->controller==Plan.child[1]){slot=2;agent=2;}
      else return EFI_ACCESS_DENIED;
    } else if(e->handle==Plan.child[0]) {
      if(Guid(&e->protocol,&TextGuid)&&e->attributes==16&&e->controller==Plan.child[0]){slot=3;agent=0;}
      else if(Guid(&e->protocol,&TextGuid)&&e->attributes==8&&e->controller==Plan.console){slot=4;agent=0;}
      else if(Guid(&e->protocol,&GopGuid)&&e->attributes==16&&e->controller==Plan.child[0]){slot=5;agent=1;}
      else if(Guid(&e->protocol,&AuxGuid)&&e->attributes==16&&e->controller==Plan.child[0]){slot=6;agent=3;}
      else return EFI_ACCESS_DENIED;
    } else if(e->handle==Plan.child[1]) {
      if(!Guid(&e->protocol,&AuxGuid)||e->attributes!=16||e->controller!=Plan.child[1])return EFI_ACCESS_DENIED;
      agent=3;
    } else continue;
    if(e->count!=1||(Plan.agent[agent]&&Plan.agent[agent]!=e->agent))return EFI_ACCESS_DENIED;
    Plan.agent[agent]=e->agent;if(slot>=0)counts[slot]++;
  }
  for(UINTN i=0;i<7;i++)if(counts[i]!=1)return EFI_ACCESS_DENIED;
  UINTN second=0;
  for(UINTN i=0;i<Original.count;i++)if(Original.edges[i].handle==Plan.child[1])second++;
  if(second!=1)return EFI_ACCESS_DENIED;
  for(UINTN i=0;i<4;i++) {
    if(Plan.agent[i]==Plan.self||Plan.agent[i]==Plan.pci||Plan.agent[i]==Plan.console)return EFI_ACCESS_DENIED;
    for(UINTN j=i+1;j<4;j++)if(Plan.agent[i]==Plan.agent[j])return EFI_ACCESS_DENIED;
    EFI_STATUS s=Driver(i);if(s!=EFI_SUCCESS)return s;
  }
  return EFI_SUCCESS;
}
EFI_STATUS GuMsiPreflight(EFI_SYSTEM_TABLE *t,EFI_HANDLE self,EFI_HANDLE pci) {
  if(Plan.mutated||Plan.ready)return EFI_ALREADY_STARTED;
  memset(&Plan,0,sizeof(Plan));Plan.table=t;Plan.self=self;Plan.pci=pci;
  if(!t||!t->BootServices||!self||!pci||Gu.handle!=pci||Gu.image!=self||Gu.bs!=t->BootServices||!Gu.profile_verified||!Gu.prom_verified||Gu.started||Gu.owned||Gu.armed)return EFI_INVALID_PARAMETER;
  EFI_BOOT_SERVICES *b=t->BootServices;
  if(!b->DisconnectController||!b->OpenProtocol||!b->CloseProtocol||!b->RaiseTPL||!b->RestoreTPL||!b->ProtocolsPerHandle||!b->OpenProtocolInformation||!b->LocateHandleBuffer||!b->HandleProtocol||!b->FreePool||!Gu.pci->Attributes)return EFI_UNSUPPORTED;
  Plan.console=t->ConsoleOutHandle;Plan.input=t->ConsoleInHandle;Plan.error=t->StandardErrorHandle;Plan.text=t->ConOut;Plan.err=t->StdErr;Plan.keys=t->ConIn;
  if(!Plan.console||Plan.console!=Plan.input||Plan.console!=Plan.error||!Plan.text||!Plan.err||!Plan.keys||!Plan.keys->ReadKeyStroke||!Plan.keys->Reset||!Plan.keys->WaitForKey)return EFI_UNSUPPORTED;
  Plan.key_reset=(UINTN)Plan.keys->Reset;Plan.key_read=(UINTN)Plan.keys->ReadKeyStroke;Plan.key_wait=Plan.keys->WaitForKey;
  Plan.text_output=(UINTN)Plan.text->OutputString;Plan.err_output=(UINTN)Plan.err->OutputString;
  Plan.keys_value=*Plan.keys;Plan.text_value=*Plan.text;Plan.err_value=*Plan.err;
  if(!Path(pci,PciPath,sizeof(PciPath))||!GuAd104ColdEntry())return EFI_NOT_READY;
  EFI_STATUS s=Snapshot(&Original,TRUE);
  if(s==EFI_SUCCESS)s=Gops(TRUE);
  if(s==EFI_SUCCESS)s=Alive();
  if(s==EFI_SUCCESS)s=Bar();
  if(s==EFI_SUCCESS)s=ReadConfig(Plan.config);
  if(s==EFI_SUCCESS)s=Gu.pci->Attributes(Gu.pci,EfiPciIoAttributeOperationGet,0,&Plan.attributes);
  UINT64 decode=(Plan.config[1]&1?EFI_PCI_IO_ATTRIBUTE_IO:0)|(Plan.config[1]&2?EFI_PCI_IO_ATTRIBUTE_MEMORY:0);
  if(s==EFI_SUCCESS&&(Plan.attributes&(EFI_PCI_IO_ATTRIBUTE_IO|EFI_PCI_IO_ATTRIBUTE_MEMORY|EFI_PCI_IO_ATTRIBUTE_BUS_MASTER))!=decode)s=EFI_NOT_READY;
  if(s==EFI_SUCCESS)s=Consumers();
  if(s==EFI_SUCCESS){s=Snapshot(&Current,FALSE);if(s==EFI_SUCCESS&&!Expected(0,FALSE))s=EFI_NOT_READY;}
  GuLog("X870E preflight: status=%#llx edges=%zu; two NVIDIA children; AMD preserved; four driver-code pins\n",(unsigned long long)s,(size_t)Original.count);
  if(s==EFI_SUCCESS)Plan.ready=TRUE;return s;
}
static BOOLEAN DecodeMask(const UINT32 c[64],UINT64 *mask) {
  UINT32 missing=Plan.config[1]&~c[1]&3;*mask=0;
  if((Plan.config[1]&6)!=2||(c[1]&4))return FALSE;
  for(UINTN i=0;i<64;i++)if((c[i]|(i==1?missing:0))!=Plan.config[i])return FALSE;
  *mask=(missing&1?EFI_PCI_IO_ATTRIBUTE_IO:0)|(missing&2?EFI_PCI_IO_ATTRIBUTE_MEMORY:0);return TRUE;
}
static EFI_STATUS ClaimRestore(VOID) {
  EFI_BOOT_SERVICES *b=Plan.table->BootServices;EFI_PCI_IO_PROTOCOL *io=NULL;UINT32 c[64];UINT64 mask=0,attrs=0,supported=0;
  EFI_TPL old=b->RaiseTPL(TPL_NOTIFY);EFI_STATUS s=ReadConfig(c);
  if(s==EFI_SUCCESS&&!DecodeMask(c,&mask))s=EFI_NOT_READY;
  if(s==EFI_SUCCESS)s=Snapshot(&Current,FALSE);
  if(s==EFI_SUCCESS&&!Expected(5,FALSE))s=EFI_NOT_READY;
  if(s==EFI_SUCCESS)s=b->OpenProtocol(Plan.pci,&PciGuid,(VOID **)&io,Plan.self,NULL,EFI_OPEN_PROTOCOL_EXCLUSIVE);
  if(s==EFI_SUCCESS){Plan.claimed=TRUE;Gu.owned=TRUE;if(io!=Gu.pci)s=EFI_COMPROMISED_DATA;}
  if(s==EFI_SUCCESS)s=io->Attributes(io,EfiPciIoAttributeOperationGet,0,&attrs);
  if(s==EFI_SUCCESS&&attrs!=(Plan.attributes&~mask))s=EFI_NOT_READY;
  if(s==EFI_SUCCESS)s=io->Attributes(io,EfiPciIoAttributeOperationSupported,0,&supported);
  if(s==EFI_SUCCESS&&(supported&mask)!=mask)s=EFI_UNSUPPORTED;
  if(s==EFI_SUCCESS)s=Snapshot(&Current,FALSE);
  if(s==EFI_SUCCESS&&!Expected(5,TRUE))s=EFI_NOT_READY;
  if(s==EFI_SUCCESS&&mask)s=io->Attributes(io,EfiPciIoAttributeOperationEnable,mask,NULL);
  b->RestoreTPL(old);
  if(s==EFI_SUCCESS)s=ReadConfig(c);
  if(s==EFI_SUCCESS&&!Equal(c,Plan.config,sizeof(c)))s=EFI_NOT_READY;
  if(s==EFI_SUCCESS)s=io->Attributes(io,EfiPciIoAttributeOperationGet,0,&attrs);
  if(s==EFI_SUCCESS&&attrs!=Plan.attributes)s=EFI_NOT_READY;
  if(s==EFI_SUCCESS)s=Bar();
  if(s==EFI_SUCCESS&&!GuAd104ColdEntry())s=EFI_NOT_READY;
  return s;
}
EFI_STATUS GuMsiRelease(VOID) {
  if(!Plan.ready||Plan.mutated||Gu.started||Gu.owned)return EFI_NOT_READY;
  EFI_BOOT_SERVICES *b=Plan.table->BootServices;
  static const UINTN agents[]={3,3,0,1,2};
  for(UINTN stage=1;stage<=5;stage++) {
    EFI_STATUS s=Alive();UINT32 c[64];
    if(s==EFI_SUCCESS)s=ReadConfig(c);
    if(s==EFI_SUCCESS&&!Equal(c,Plan.config,sizeof(c)))s=EFI_NOT_READY;
    if(s==EFI_SUCCESS&&!GuAd104ColdEntry())s=EFI_NOT_READY;
    if(s==EFI_SUCCESS)s=Gops(FALSE);
    EFI_TPL old=b->RaiseTPL(TPL_NOTIFY);
    if(s==EFI_SUCCESS)s=Snapshot(&Current,FALSE);
    if(s==EFI_SUCCESS&&!Expected(stage-1,FALSE))s=EFI_NOT_READY;
    if(s==EFI_SUCCESS)s=Driver(agents[stage-1]);
    b->RestoreTPL(old);
    if(s!=EFI_SUCCESS||Gu.fault)return s==EFI_SUCCESS?EFI_DEVICE_ERROR:s;
    GuLog("X870E detach intent stage=%zu: exact NVIDIA controller only\n",(size_t)stage);
    if(Gu.fault)return EFI_DEVICE_ERROR;
    Plan.mutated=TRUE;
    s=b->DisconnectController(stage==5?Plan.pci:Plan.child[stage==1?1:0],Plan.agent[agents[stage-1]],NULL);
    GuLog("X870E detach return stage=%zu status=%#llx\n",(size_t)stage,(unsigned long long)s);
    if(s!=EFI_SUCCESS||Gu.fault)return s==EFI_SUCCESS?EFI_DEVICE_ERROR:s;
    Plan.stage=stage;s=Snapshot(&Current,FALSE);
    if(s==EFI_SUCCESS&&!Expected(stage,FALSE))s=EFI_NOT_READY;
    if(s==EFI_SUCCESS)s=Alive();
    if(s==EFI_SUCCESS)s=Gops(FALSE);
    if(s!=EFI_SUCCESS)return s;
  }
  EFI_STATUS s=ClaimRestore();
  if(s==EFI_SUCCESS&&!GuMsiReleased())s=EFI_NOT_READY;
  return s;
}
BOOLEAN GuMsiReleased(VOID) {
  UINT32 c[64];UINT64 attrs=0;
  return Plan.ready&&Plan.stage==5&&Plan.claimed&&Gu.owned&&!Gu.fault&&Alive()==EFI_SUCCESS&&Gops(FALSE)==EFI_SUCCESS&&ReadConfig(c)==EFI_SUCCESS&&ConfigStable(c)&&
    Gu.pci->Attributes(Gu.pci,EfiPciIoAttributeOperationGet,0,&attrs)==EFI_SUCCESS&&attrs==Plan.attributes&&Snapshot(&Current,FALSE)==EFI_SUCCESS&&Expected(5,TRUE);
}
EFI_STATUS GuMsiFinish(VOID) {
  if(!GuSafeToReturn()||!Gu.quiesced||Gu.flrs!=1||!GuMsiReleased())return EFI_NOT_READY;
  EFI_STATUS s=Plan.table->BootServices->CloseProtocol(Plan.pci,&PciGuid,Plan.self,NULL);
  if(s!=EFI_SUCCESS)return s;
  Plan.claimed=FALSE;Gu.owned=FALSE;
  s=Snapshot(&Current,FALSE);if(s==EFI_SUCCESS&&!Expected(5,FALSE))s=EFI_NOT_READY;return s;
}
BOOLEAN GuMsiMutated(VOID){return Plan.mutated;}
BOOLEAN GuMsiReadyForWindows(VOID) {
  if(!Plan.ready||Plan.stage!=5||Plan.claimed||Gu.owned||Gu.fault||!Gu.quiesced||Gu.flrs!=1||!GuSafeToReturn())return FALSE;
  UINT32 c[64];
  if(Alive()!=EFI_SUCCESS||Gops(FALSE)!=EFI_SUCCESS||ReadConfig(c)!=EFI_SUCCESS||!ConfigStable(c)||Snapshot(&Current,FALSE)!=EFI_SUCCESS||!Expected(5,FALSE))return FALSE;
  UINT32 chip=GuRead32(0),cpu=GuRead32(0x110100),hi=GuRead32(0x1fa828),ss0=GuRead32(0x82381c),ss1=GuRead32(0x823820),readout=GuRead32(0x823818),plm=GuRead32(0x823804);
  return !Gu.fault&&chip==GU_BOARD_CHIP&&(cpu&0x10)&&cpu!=0xffffffff&&(cpu&0xffff0000)!=0xbadf0000&&!(hi>>4)&&ss0==0x88888888&&ss1==8&&!(readout&0x07ffffff)&&readout!=0xffffffff&&plm==0xffffffff;
}
