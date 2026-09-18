// ---------------------------------------------------------------------------
// stratum_client.c -- complete stratum client for the mining SoC.
// Runs on the BOOM core under Linux. Talks JSON-RPC over TCP to a pool,
// builds block headers from mining.notify, drives the SHA-256d accelerator
// over MMIO, and submits shares.
//
//   pool  ---- mining.subscribe ---->  extranonce1, extranonce2_size
//   pool  ---- mining.authorize ---->  ok
//   pool  ---- mining.notify ------->  job: coinbase + merkle branch
//   pool  ---- mining.set_difficulty  share target = diff1 / diff
//   miner -- mining.submit -------->  [worker, job_id, en2, ntime, nonce]
//
// Endianness conventions (validated against the genesis-block KAT):
//   prevhash (stratum): display order (BE) -> header: reverse ALL 32 bytes
//   version/nbits/ntime: display hex uint32 -> header: reverse the 4 bytes (LE)
//   merkle root: we compute it -> header uses it DIRECTLY (internal order)
//   submit nonce: "%08x" of the uint32 nonce value (display form)
// ---------------------------------------------------------------------------
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <netdb.h>
#include <sys/mman.h>
#include <sys/socket.h>

// ---------------------------------------------------------------- SHA-256 --
typedef struct { uint32_t h[8]; uint64_t n; uint8_t b[64]; int blen; } sha256_t;
static const uint32_t K[64] = {
 0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
 0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
 0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
 0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
 0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
 0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
 0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
 0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
static uint32_t rotr(uint32_t x,int n){return (x>>n)|(x<<(32-n));}
static void sha256_blk(sha256_t*s,const uint8_t*p){
  uint32_t w[64]; int t;
  for(t=0;t<16;t++) w[t]=(uint32_t)p[4*t]<<24|(uint32_t)p[4*t+1]<<16|(uint32_t)p[4*t+2]<<8|p[4*t+3];
  for(;t<64;t++){uint32_t x=w[t-2],y=w[t-15];
    w[t]=(rotr(x,17)^rotr(x,19)^(x>>10))+w[t-7]+(rotr(y,7)^rotr(y,18)^(y>>3))+w[t-16];}
  uint32_t a=s->h[0],b=s->h[1],c=s->h[2],d=s->h[3],e=s->h[4],f=s->h[5],g=s->h[6],h=s->h[7];
  for(t=0;t<64;t++){uint32_t t1=h+(rotr(e,6)^rotr(e,11)^rotr(e,25))+((e&f)^(~e&g))+K[t]+w[t];
    uint32_t t2=(rotr(a,2)^rotr(a,13)^rotr(a,22))+((a&b)^(a&c)^(b&c));
    h=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;}
  s->h[0]+=a;s->h[1]+=b;s->h[2]+=c;s->h[3]+=d;s->h[4]+=e;s->h[5]+=f;s->h[6]+=g;s->h[7]+=h;
}
static void sha256_init(sha256_t*s){static const uint32_t i[8]={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};memcpy(s->h,i,sizeof i);s->n=0;s->blen=0;}
static void sha256_upd(sha256_t*s,const uint8_t*p,size_t l){s->n+=l*8;while(l--){s->b[s->blen++]=*p++;if(s->blen==64){sha256_blk(s,s->b);s->blen=0;}}}
static void sha256_fin(sha256_t*s,uint8_t*d){uint64_t bits=s->n;s->n=0;uint8_t pad=0x80;sha256_upd(s,&pad,1);uint8_t z=0;while(s->blen!=56)sha256_upd(s,&z,1);uint8_t le[8];for(int i=0;i<8;i++)le[i]=(uint8_t)(bits>>(8*(7-i)));/* SHA-256 length field is 64-bit BIG-ENDIAN */sha256_upd(s,le,8);for(int i=0;i<8;i++){d[4*i]=s->h[i]>>24;d[4*i+1]=s->h[i]>>16;d[4*i+2]=s->h[i]>>8;d[4*i+3]=s->h[i];}}
static void dsha(const uint8_t*in,size_t ilen,uint8_t out[32]){uint8_t t[32];sha256_t s;sha256_init(&s);sha256_upd(&s,in,ilen);sha256_fin(&s,t);sha256_init(&s);sha256_upd(&s,t,32);sha256_fin(&s,out);}

// ------------------------------------------------------------ hex helpers --
static int hexv(int c){if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;if(c>='A'&&c<='F')return c-'A'+10;return -1;}
static int hex2bin(const char*h,uint8_t*out,int maxout){int n=0;while(h[0]&&h[1]&&hexv(h[0])>=0&&hexv(h[1])>=0){if(n>=maxout)return -1;out[n++]=(uint8_t)(hexv(h[0])<<4|hexv(h[1]));h+=2;}return (*h)?-1:n;}   /* only test h[0]: h[1] reads past the NUL for even-length strings (UB) */
static void bin2hex(const uint8_t*b,int n,char*out){for(int i=0;i<n;i++){out[2*i]="0123456789abcdef"[b[i]>>4];out[2*i+1]="0123456789abcdef"[b[i]&15];}out[2*n]=0;}
static void revb(uint8_t*b,int n){for(int i=0;i<n/2;i++){uint8_t t=b[i];b[i]=b[n-1-i];b[n-1-i]=t;}}
static void rev4(uint8_t*b){uint8_t t=b[0];b[0]=b[3];b[3]=t;t=b[1];b[1]=b[2];b[2]=t;}

// ------------------------------------------------------- mini JSON parser --
// Fixed-size DOM, no malloc. Enough for stratum messages.
typedef enum {J_NULL,J_FALSE,J_TRUE,J_NUM,J_STR,J_ARR,J_OBJ} jtype;
typedef struct { jtype t; const char*s; int len; int child,next; } jnode;
typedef struct { jnode nd[2048]; int n; const char*p; int err; } jp;
static int jadd(jp*j,jtype t,const char*s,int l){ if(j->n>=2048){j->err=1;return -1;} int i=j->n++; j->nd[i]=(jnode){t,s,l,-1,-1}; return i; }
static void jws(jp*j){ while(*j->p==' '||*j->p=='\t'||*j->p=='\r'||*j->p=='\n') j->p++; }
static int jval(jp*j);
static int jstr(jp*j){ const char*s=++j->p; int l=0; while(*j->p&&*j->p!='"'){ if(*j->p=='\\'&&j->p[1]){j->p+=2;l+=2;} else {j->p++;l++;} } if(*j->p=='"')j->p++; return jadd(j,J_STR,s,l); }
static int jlit(jp*j,const char*w,jtype t){ int l=(int)strlen(w); if(!strncmp(j->p,w,l)){j->p+=l;return jadd(j,t,w,0);} j->err=1; return -1; }
static int jarr(jp*j){ int me=jadd(j,J_ARR,j->p,0); j->p++; int last=-1; jws(j); if(*j->p==']'){j->p++;return me;}
  for(;;){ int c=jval(j); if(c<0)return -1; if(last<0)j->nd[me].child=c; else j->nd[last].next=c; last=c; jws(j); if(*j->p==','){j->p++;continue;} if(*j->p==']'){j->p++;return me;} j->err=1; return -1; } }
static int jobj(jp*j){ int me=jadd(j,J_OBJ,j->p,0); j->p++; int lastv=-1; jws(j); if(*j->p=='}'){j->p++;return me;}
  for(;;){ jws(j); if(*j->p!='"'){j->err=1;return -1;} int k=jstr(j); jws(j); if(*j->p==':')j->p++; int c=jval(j); if(c<0)return -1; if(lastv<0)j->nd[me].child=k; else j->nd[lastv].next=k; j->nd[k].next=c; lastv=c; jws(j); if(*j->p==','){j->p++;continue;} if(*j->p=='}'){j->p++;return me;} j->err=1; return -1; } }
static int jval(jp*j){ jws(j); char c=*j->p;
  if(c=='"')return jstr(j); if(c=='[')return jarr(j); if(c=='{')return jobj(j);
  if(c=='t')return jlit(j,"true",J_TRUE); if(c=='f')return jlit(j,"false",J_FALSE); if(c=='n')return jlit(j,"null",J_NULL);
  const char*s=j->p; while(*j->p&&strchr("-+.0123456789eE",*j->p))j->p++; return jadd(j,J_NUM,s,(int)(j->p-s)); }
static int jparse(jp*j,const char*t){ j->n=0;j->err=0;j->p=t; jws(j); int r=jval(j); return (j->err||r<0)?-1:r; }
static int jget(const jp*j,int obj,const char*key){ if(obj<0||j->nd[obj].t!=J_OBJ)return -1; for(int c=j->nd[obj].child;c>=0;c=j->nd[c].next){ if(j->nd[c].t==J_STR&&j->nd[c].len==(int)strlen(key)&&!strncmp(j->nd[c].s,key,j->nd[c].len)) return j->nd[c].next; } return -1; }
static int jidx(const jp*j,int arr,int i){ if(arr<0||j->nd[arr].t!=J_ARR)return -1; int c=j->nd[arr].child; while(i-->0&&c>=0)c=j->nd[c].next; return c; }
static void jtext(const jp*j,int n,char*out,int sz){ // unescape into out
  if(n<0){*out=0;return;} const char*s=j->nd[n].s; int l=j->nd[n].len,o=0;
  for(int i=0;i<l&&o<sz-1;i++){ if(s[i]=='\\'&&i+1<l){ char e=s[++i]; if(e=='n')out[o++]='\n'; else if(e=='t')out[o++]='\t'; else out[o++]=e; } else out[o++]=s[i]; } out[o]=0; }
static double jnum(const jp*j,int n){ char b[64]; jtext(j,n,b,sizeof b); return n<0?0:strtod(b,0); }

// ------------------------------------------------------ accelerator regs ---
#define MINING_BASE 0x10020000UL
#define R_CTRL 0x00
#define R_STATUS 0x04
#define R_IRQ_EN 0x08
#define R_NONCE_START 0x0C
#define R_BLOCK1 0x10
#define R_B2W 0x50
#define R_NONCE_STRIDE 0x5C
#define R_TARGET 0x60
#define R_NONCE_FOUND 0x80
#define R_FOUND 0xA4
static volatile uint32_t *REG;
static int accel_init(void){ int fd=open("/dev/mem",O_RDWR|O_SYNC); if(fd<0){perror("/dev/mem");return -1;}
  void*p=mmap(0,0x1000,PROT_READ|PROT_WRITE,MAP_SHARED,fd,MINING_BASE); if(p==MAP_FAILED){perror("mmap");return -1;} REG=(volatile uint32_t*)p; return 0; }
static void wr(uint32_t o,uint32_t v){REG[o/4]=v;}
static uint32_t rd(uint32_t o){return REG[o/4];}
static void ctrl(uint32_t v){ wr(R_CTRL,v); while(rd(R_CTRL)); } // pulses self-clear; brief spin

static void accel_load(const uint8_t hdr[80], const uint8_t target[32]){
  for(int i=0;i<16;i++) wr(R_BLOCK1+4*i,(uint32_t)hdr[4*i]<<24|hdr[4*i+1]<<16|hdr[4*i+2]<<8|hdr[4*i+3]);
  for(int i=0;i<3;i++)  wr(R_B2W+4*i,(uint32_t)hdr[64+4*i]<<24|hdr[64+4*i+1]<<16|hdr[64+4*i+2]<<8|hdr[64+4*i+3]);
  for(int i=0;i<8;i++)  wr(R_TARGET+4*i,(uint32_t)target[4*i]<<24|target[4*i+1]<<16|target[4*i+2]<<8|target[4*i+3]);
  ctrl(0x2); while(rd(R_STATUS)&1); // load template: engines compute midstate
}
static bool accel_scan(uint32_t start,uint32_t*nonce){
  wr(R_NONCE_START,start); ctrl(0x1);
  for(;;){ if(rd(R_FOUND)){ *nonce=rd(R_NONCE_FOUND); ctrl(0x4); return true; }
           if(!(rd(R_STATUS)&1)) return false; }
}

// ------------------------------------------------------ stratum protocol ---
typedef struct {
  char job_id[64];
  uint8_t header[80];      // nonce field zeroed; we scan it in hardware
  uint8_t target[32];      // big-endian display form (MSB first)
  char ntime_hex[9];       // display form, for submit
  uint8_t extranonce2[8]; int en2_len;
} job_t;

static char g_en1[64]; static int g_en2_size=4;
static char g_worker[128]="worker";

// target = diff1_target / diff, diff1 = 0xffff << 208. Simple 256/64 division.
static void share_target(double diff,uint8_t out[32]){
  uint64_t q[4]={0,0,0,(uint64_t)0xffff<<16}; // diff1 limbs, limb3 = MS
  uint64_t d=(uint64_t)diff; if(!d)d=1;
  unsigned __int128 rem=0; uint64_t qq[4]={0,0,0,0};
  for(int bit=255;bit>=0;bit--){ rem=(rem<<1)|((q[bit/64]>>(bit%64))&1);
    if(rem>=d){ rem-=d; qq[bit/64]|=(uint64_t)1<<(bit%64); } }
  for(int i=0;i<4;i++){ out[8*i+0]=qq[3-i]>>56; out[8*i+1]=qq[3-i]>>48; out[8*i+2]=qq[3-i]>>40; out[8*i+3]=qq[3-i]>>32;
                        out[8*i+4]=qq[3-i]>>24; out[8*i+5]=qq[3-i]>>16; out[8*i+6]=qq[3-i]>>8;  out[8*i+7]=qq[3-i]; }
}

// Build the 80-byte header for one job. See endianness conventions in header.
static int build_job(const jp*j,int params,job_t*job){
  char tmp[256];
  int v;
  v=jidx(j,params,0); jtext(j,v,job->job_id,sizeof job->job_id);
  uint8_t prev[32]; v=jidx(j,params,1); jtext(j,v,tmp,sizeof tmp);
  if(hex2bin(tmp,prev,32)!=32)return -1; revb(prev,32);
  v=jidx(j,params,2); jtext(j,v,tmp,sizeof tmp); uint8_t coinb1[256]; int cb1=hex2bin(tmp,coinb1,sizeof coinb1); if(cb1<0)return -1;
  v=jidx(j,params,3); jtext(j,v,tmp,sizeof tmp); uint8_t coinb2[256]; int cb2=hex2bin(tmp,coinb2,sizeof coinb2); if(cb2<0)return -1;
  v=jidx(j,params,4); // merkle branch, internal byte order (as pools send)
  uint8_t cb[600]; int cbl=0;
  memcpy(cb,coinb1,cb1); cbl+=cb1;
  { uint8_t e[8]; int en=hex2bin(g_en1,e,sizeof e); memcpy(cb+cbl,e,en); cbl+=en; }
  memcpy(cb+cbl,job->extranonce2,job->en2_len); cbl+=job->en2_len;
  memcpy(cb+cbl,coinb2,cb2); cbl+=cb2;
  uint8_t cur[32]; dsha(cb,cbl,cur);
  for(int i=0;;i++){ int b=jidx(j,v,i); if(b<0)break; jtext(j,b,tmp,sizeof tmp);
    uint8_t node[32]; if(hex2bin(tmp,node,32)!=32)return -1;
    uint8_t pair[64]; memcpy(pair,cur,32); memcpy(pair+32,node,32); dsha(pair,64,cur); } // coinbase is leaf 0: always left
  // version (param 5), nbits (6), ntime (7)
  uint8_t version[4]={0x00,0x00,0x00,0x20}; // fallback 0x20000000
  v=jidx(j,params,5); if(v>=0){ jtext(j,v,tmp,sizeof tmp); if(hex2bin(tmp,version,4)==4)rev4(version); }
  uint8_t nbits[4]; v=jidx(j,params,6); jtext(j,v,tmp,sizeof tmp); if(hex2bin(tmp,nbits,4)!=4)return -1; rev4(nbits);
  uint8_t ntime[4]; v=jidx(j,params,7); jtext(j,v,tmp,sizeof tmp); if(hex2bin(tmp,ntime,4)!=4)return -1; rev4(ntime);
  memcpy(job->ntime_hex, j->nd[jidx(j,params,7)].s, 8); job->ntime_hex[8]=0;
  uint8_t*h=job->header;
  memcpy(h+0,version,4); memcpy(h+4,prev,32); memcpy(h+36,cur,32);
  memcpy(h+68,ntime,4); memcpy(h+72,nbits,4); memset(h+76,0,4);
  return 0;
}

// ------------------------------------------------------------ stratum io ---
static int sk;
static int sconnect(const char*host,const char*port){
  struct addrinfo hi={0},*ai; hi.ai_socktype=SOCK_STREAM;
  if(getaddrinfo(host,port,&hi,&ai))return -1;
  sk=socket(ai->ai_family,SOCK_STREAM,0); if(sk<0)return -1;
  if(connect(sk,ai->ai_addr,ai->ai_addrlen)){close(sk);return -1;} return 0; }
static int sreadline(char*buf,int n){ // returns length or <=0
  int i=0; while(i<n-1){ uint8_t c; int r=(int)read(sk,&c,1); if(r<=0)return r; if(c=='\n')break; buf[i++]=c; } buf[i]=0; return i; }
static void ssendf(const char*fmt,...){ char b[2048]; __builtin_va_list ap; __builtin_va_start(ap,fmt); vsnprintf(b,sizeof b,fmt,ap); __builtin_va_end(ap); write(sk,b,strlen(b)); }

// ---------------------------------------------------------------- main -----
#ifndef STRATUM_TEST
static int msgid=1;
int main(int argc,char**argv){
  if(argc<3){ fprintf(stderr,"usage: %s <pool:port> <worker[.miner]> [password]\n",argv[0]); return 1; }
  const char*colon=strrchr(argv[1],':');
  char host[256],port[16]; if(!colon){fprintf(stderr,"need host:port\n");return 1;}
  snprintf(host,sizeof host,"%.*s",(int)(colon-argv[1]),argv[1]); snprintf(port,sizeof port,"%s",colon+1);
  snprintf(g_worker,sizeof g_worker,"%s",argv[2]);
  if(accel_init())return 1;

  if(sconnect(host,port)){perror("connect");return 1;}
  ssendf("{\"id\":%d,\"method\":\"mining.subscribe\",\"params\":[\"mining-soc/0.1\"]}\n",msgid++);
  ssendf("{\"id\":%d,\"method\":\"mining.authorize\",\"params\":[\"%s\",\"%s\"]}\n",msgid++,g_worker,argc>3?argv[3]:"x");

  uint32_t en2ctr=0; job_t job; memset(&job,0,sizeof job); job.en2_len=g_en2_size; double diff=1.0;
  char line[4096];
  for(;;){
    int r=sreadline(line,sizeof line); if(r<=0){ sleep(5); if(sconnect(host,port))sleep(30); continue; }
    jp j; int root=jparse(&j,line); if(root<0)continue;
    char method[64]; jtext(&j,jget(&j,root,"method"),method,sizeof method);
    int params=jget(&j,root,"params"), idn=jget(&j,root,"id"), res=jget(&j,root,"result");
    if(!strcmp(method,"mining.set_difficulty")){ diff=jnum(&j,jidx(&j,params,0)); continue; }
    if(!strcmp(method,"mining.notify")){
      for(int i=0;i<g_en2_size;i++) job.extranonce2[i]=(uint8_t)(en2ctr>>(8*(g_en2_size-1-i))); en2ctr++;
      if(build_job(&j,params,&job)){fprintf(stderr,"bad notify\n");continue;}
      share_target(diff,job.target);
      accel_load(job.header,job.target);
      for(uint64_t w=0;;w++){ uint32_t nonce;
        if(accel_scan((uint32_t)(w*8u*0x02000000u),&nonce)){
          char en2h[32]; bin2hex(job.extranonce2,job.en2_len,en2h);
          ssendf("{\"id\":%d,\"method\":\"mining.submit\",\"params\":[\"%s\",\"%s\",\"%s\",\"%s\",\"%08x\"]}\n",
                 msgid++,g_worker,job.job_id,en2h,job.ntime_hex,nonce);
          fprintf(stderr,"share! nonce=%08x\n",nonce);
        }
        if(w>=(0x100000000ULL/(8u*0x02000000u)))break; // wrapped the space
      }
      continue;
    }
    if(idn>=0&&res>=0){ char t[8]; jtext(&j,res,t,sizeof t); if(!strcmp(t,"true"))fprintf(stderr,"auth ok\n"); }
  }
}
#else
// ---- test harness: feed pool messages on stdin, print computed artifacts --
int main(void){
  strcpy(g_en1,"c0ffee00"); g_en2_size=4;
  char line[4096]; static job_t job; memset(&job,0,sizeof job); job.en2_len=4;
  job.extranonce2[3]=1;   // extranonce2 = 0x00000001
  while(fgets(line,sizeof line,stdin)){
    if(!strncmp(line,"SUBSCRIBE_RESULT ",17)){ jp j; if(jparse(&j,line+17)>=0){
        char b[64]; int res=jget(&j,0,"result");
        jtext(&j,jidx(&j,res,1),b,sizeof b);
        printf("EN1 %s EN2SIZE %d\n", b, (int)jnum(&j,jidx(&j,res,2))); } }
    else if(!strncmp(line,"NOTIFY ",7)){ jp j; int root=jparse(&j,line+7);
      if(root<0){printf("PARSE_ERR\n");continue;}
      if(build_job(&j,jget(&j,root,"params"),&job)){printf("BUILD_ERR\n");continue;}
      char mh[65],hh[161]; bin2hex(job.header+36,32,mh); bin2hex(job.header,80,hh);
      printf("JOBID %s\nMERKLE %s\nHEADER %s\n", job.job_id, mh, hh); }
    else if(!strncmp(line,"DIFF ",5)){ uint8_t t[32]; share_target(atof(line+5),t);
      char h[65]; bin2hex(t,32,h); printf("TARGET %s\n",h); }
    else if(!strncmp(line,"ABC",3)){ uint8_t t[32]; dsha((const uint8_t*)"abc",3,t);
      char h[65]; bin2hex(t,32,h); printf("ABC %s\n",h); }
  }
  return 0;
}
#endif
