/*
 * This file is part of zed, the simple, powerful, fast, small
 * text editor (c) 1997 by Sandro Serafini.
 * The author may be contacted by his email address,
 * zed@iam.it
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/* lowl.cc - modulo con le funzioni a basso livello */

//#define SUPERDEBUG

/***************************************************************************/

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <errno.h>
#include <regex.h>
#include <sys/stat.h>
#ifdef X11
 #include <X11/Xlib.h>
#endif

#ifdef _AIX
 #include <sys/select.h>
 #include <strings.h>
#endif

#include "zed.h"

#ifdef MSDOS
 #include <dir.h>
 #include <dos.h>
 #include <sys\stat.h>
 #include <sys\time.h>
 #include <errno.h>
 #include <go32.h>
 #include <sys\farptr.h>
 #include <dpmi.h>
#endif

#ifdef UNIX
 #include <sys/ioctl.h>
 #include <sys/time.h>
// #include <sys/types.h>
 #include <termios.h>

 struct termios ot0,ot1,nt0,nt1;
#endif


int pattr=1000;

#ifdef X11
 #include <X11/keysym.h>
#endif

/***************************************************************************/

//int scx,scy,scx1,scy1; // Save Cursor position
int crx,cry;             // posizione del cursore corrente
int goline(int &l,char *str,int i);
void addmacro(unsigned char cmd,char chr);

char *hints[]=
{
 "Hint: An empty string brings up the file selector.",
 "Hint: A directory name or some wildcard brings up the file selector.",
 "Example: Type \"*.cc *.h\" to select one (or all) your .cc and .h files.",
 "Hint: Type \"<g\" as filename to select your local config file.",
 "Hint: Zed is simple to configure.",
 "Hint: You can go to a certain C function asking Zed to list them (g_function).",
 "Hint: Type \"<h\" as filename to select the help.",
 "Hint: you can shift left or right the lines within a block (b_shl,b_shr).",
 "Hint: read zed.doc file!",
 #ifdef UNIX
  "Hint: You can exec a command in background and redirect its output to a window",
 #endif
 #ifdef __linux
  "Note: if you use Zed in a remote text terminal, shiftstatus can\'t be retrieved.",
 #endif
 #if defined __linux || X11
  "Hint: You want that shift-escape + alt-shift-$ exits Zed? Simply configure it!",
 #endif
 "Hint: Try document mode on paragraphs of quoted lines of mail or news.",
 "Hint: b_toggle to toggle block mode between normal and column.",
 "Hint: e_stmacro to create a runtime macro; e_wrmacro to save it to a file.",
 0
};

/***************************************************************************/

void _getcwd(char *path,int max,char drive)
{
 path[0]='\0';

 if (drive==0)
 { if (!getcwd(path,max)) path[0]='\0'; }
 #ifdef MSDOS
  else
  {
   int sd=getdisk(); // save drive
   int dto=toupper(drive)-'A';
   if (setdisk(getdisk())<dto) path[0]='\0';
   else
   {
    setdisk(dto);
    if (!getcwd(path,max)) path[0]='\0';
    setdisk(sd);
   }
  }
  if (path[0]!='\0' && path[1]!=':')
  { output("getcwd without drive!\n"); exit(100); }
 #endif
}

#ifdef MSDOS

int _chdir(char *path)
{
 int err=0;

 if (path[1]==':')
 {
  int dto=toupper(path[0])-'A';
  if (setdisk(getdisk())<dto) err=1;
  else
  {
   setdisk(dto);
   err=chdir(path+2);
  }
 } else err=chdir(path);
 return(err);
}

#endif

/***************************************************************************/

void preinit(void)
{
 #if defined UNIX && !defined X11
  tcgetattr(0,&ot0); tcgetattr(0,&nt0);
  tcgetattr(1,&ot1); tcgetattr(1,&nt1);

  nt0.c_lflag=0;
  nt0.c_iflag=0;
  nt0.c_oflag=0;
  nt0.c_cc[VMIN]=1;
  nt0.c_cc[VTIME]=0;

  tcsetattr(0,TCSANOW,&nt0);

  nt1.c_lflag=0;
  nt1.c_iflag=0;
  nt1.c_oflag=0;
  nt1.c_cc[VMIN]=1;
  nt1.c_cc[VTIME]=0;

  tcsetattr(1,TCSANOW,&nt1);
 #endif

 #ifdef SUPERDEBUG
  unlink("/tmp/savekeys");
 #endif
}

/***************************************************************************/

void myput(char *str,int y)
{
 int i,l=strlen(str);

 for (i=0; i<l; i++)
  putchr(str[i]^0xaa,i,y,config.col[6]);
}

/***************************************************************************/

int init(int upd)
{
 int nmx,nmy;

 if (upd && config.fixx && config.fixy) return(0);

 #ifdef MSDOS
  vidsel=_go32_conventional_mem_selector();
  vidoff=_go32_info_block.linear_address_of_primary_screen&0xfffff;
  vidmyds=_go32_my_ds();

//  if (config.ansi==0 && _go32_info_block.run_mode==_GO32_RUN_MODE_DPMI)
  config.ansi=128; // correggo caso DPMI

  nmx=_farpeekb(vidsel,0x44a);
  nmy=_farpeekb(vidsel,0x484)+1;
 #elif defined X11
  nmx=MyWinInfo.cwidth; nmy=MyWinInfo.cheight;
 #else
  struct winsize wind_struct;

  if ((-1==ioctl(0,TIOCGWINSZ,&wind_struct))
   && (-1==ioctl(1,TIOCGWINSZ,&wind_struct))
   && (-1==ioctl(2,TIOCGWINSZ,&wind_struct)))
  { nmy=0; nmx=0; }
  else
  { nmx=(int)wind_struct.ws_col; nmy=(int)wind_struct.ws_row; }

  if (nmx==0)
  {
   char *p;

   p=getenv("COLUMNS");
   if (p==0) { output("\n\rPlease set/export $COLUMNS/$LINES !\n\r"); return(1); }
   else nmx=atoi(p);
  }

  if (nmy==0)
  {
   char *p;

   p=getenv("LINES");
   if (p==0) { output("\n\rPlease set/export $LINES !\n\r"); return(1); }
   else nmy=atoi(p);
  }
 #endif

 if (upd)
  if (!((!config.fixx && config.maxx!=nmx)
     || (!config.fixy && config.maxy!=nmy))) return(0);

 if (nmx<80 || nmy<4 || nmx>MAXVIDX)
 {
  if (config.ansi>0 && config.ansi<128) output("\x1b[0;10m\x1b[2J");
  if (nmx<=MAXVIDX)
  {
   output("Text window too little(");
   outint(nmx);
   output("x");
   outint(nmy);
   output("), min 80x4\n\r");
  }
  else
  {
   output("Text window too big(");
   outint(nmx);
   output("x");
   outint(nmy);
   output("), max ");
   outint(MAXVIDX-2);
   output(" columns\n\r");
  }

  if (!upd)
  {
   deinit();
   exit(2);
  }
  else
  { output("Please Resize !\n\r"); return(-1); }
 }

 if (!config.fixx) config.maxx=nmx;
 if (!config.fixy) config.maxy=nmy;

 if (upd)
 {
  #ifdef MSDOS
   if (config.ansi) { delete vbuf; }
  #else
   delete vbuf;
  #endif
  delete avideo;
  delete astat;
 }

 vbuf=0; avideo=0; astat=0;

 #ifdef MSDOS
 if (config.ansi)
 {
 #endif
  if (((vbuf=(short int *)calloc(config.maxx*config.maxy,2))==0) ||
      ((avideo=(short int *)calloc(config.maxx*config.maxy,2))==0) ||
      ((astat=(_modif *)calloc(sizeof(_modif),config.maxy+1))==0))
  { output("NoMem!\n"); deinit(); exit(1); }

  int i;

  for (i=0; i<config.maxy; i++)
  { astat[i].modif=1; astat[i].da=0; astat[i].a=config.maxx-1; }
 #ifdef MSDOS
 }
 else
 {
  vbuf=(short int *)
   ((_go32_info_block.linear_address_of_primary_screen&0xfffff)|0xe0000000);
 }
 #endif
 clearscr();

 block.blk=0;

 #if !defined X11
  if (upd) while(querykey()) gettst3();
 #endif

 return(0);
}

/***************************************************************************/

void printlogo(void)
{
 static char *titl[9]=
 {
  " ______",
  "[____  ]         _  _______________________",
  "    / /  _    _ ||",
  "   / /  / \\  / \\||    MsDos & Unix editor",
  "  / /  ( __)(    |    by  Sandro Serafini",
  " / /___ \\__  \\_/_|              (c) 1995-97",
  "[______]            _______________________",
  "                       Press F1 for help",
  0
 };

 int i;

 for (i=0; titl[i]; i++)
 {
  putstr(titl[i],0,3,i+4,config.col[6]);
 }

 putstr(retversion(),0,14,10,config.col[5]);

 struct timeval tv;

 for (i=0; hints[i]; i++);

 gettimeofday(&tv,NULL);
 srandom(tv.tv_sec);

 putstr(hints[random()%i],0,0,13,config.col[1]);

 #ifndef X11
  char *p;

  switch(config.ansi)
  {
   case 0 : p="Direct mode"; break;
   case 1 : p="Ansi color mode"; break;
   case 2 : p="Ansi mono mode"; break;
   case 3 : p="VT100/linux mode"; break;

   case 128:p="DPMI mode"; break;
   default: p="Unknow mode"; break;
  }

  putstr(p,0,26,10,config.col[6]);
 #else
  putstr("X11 mode",0,26,10,config.col[6]);
 #endif
}

/***************************************************************************/

void deinit(void)
{
 #if defined UNIX && !defined X11
  tcsetattr(0, TCSANOW, &ot0);
  tcsetattr(1, TCSANOW, &ot1);
 #endif
}

/***************************************************************************/
// se update=1 riscrive il cursore
// se update=2 cancella il cursore

void vsetcur(int x,int y,int update)
{
 if (stopupdate) return;

 if (update) { x=crx; y=cry; }

 #ifdef MSDOS
 if (config.ansi>0 && config.ansi<128)
 {
 #endif
 #if defined X11
  int xx,yy;
  int ch;
  char str[2];

  if (zedmapped)
  {
   yy=cry*MyWinInfo.fheight+MyWinInfo.fascent;
   xx=crx*MyWinInfo.fwidth;
   ch=avideo[cry*config.maxx+crx];

   if (ch!=-1)
   {
    str[0]=ch&0xff; str[1]='\0';
    if ((ch&0x4000)==0)
     XDrawImageString(display,zed_win,gcontext[((ch&0xff00)>>8)],xx,yy,str,1);
   }

   if (update!=2)
   {
    yy=y*MyWinInfo.fheight+MyWinInfo.fascent;
    xx=x*MyWinInfo.fwidth;
    ch=avideo[y*config.maxx+x];

    str[0]=ch&0xff; str[1]='\0';

    if ((ch&0x4000)==0)
    {
     XDrawImageString(display,zed_win,gcursor,xx,yy,str,1);
    }
   } else ch=-1;
  }
 #else
  char strout[20];

  sprintf(strout,"\x1b[%i;%iH",y+1,x+1);
  outansi(strout);
 #endif
 #ifdef MSDOS
 }
 else
 {
  _go32_dpmi_registers regs;
  regs.x.dx=x;
  regs.x.ax=y;
  regs.h.dh=regs.h.al;
  regs.h.ah=0x2;
  regs.h.bh=0;
  regs.x.ss=regs.x.sp=0;
  _go32_dpmi_simulate_int(0x10,&regs);
 }
 #endif
 crx=x; cry=y;
}

/***************************************************************************/

void vsavecur(int *scx,int *scy)
{
 *scx=crx; *scy=cry;
}

/***************************************************************************/

void vrestcur(int &scx,int &scy)
{
 vsetcur(scx,scy);
}

/***************************************************************************/
// sistema una linea (ansi mode)

int goline(int &l,char *str,int i)
{
 int l2,j;

 l2=config.maxx*i;

 if (astat[i].modif==1)
 {
  for (j=astat[i].da; j<=astat[i].a;  j++)
   if (vbuf[l2+j]==avideo[l2+j]) astat[i].da++; else break;
  for (j=astat[i].a;  j>=astat[i].da; j--)
   if (vbuf[l2+j]==avideo[l2+j]) astat[i].a--;  else break;
  if (astat[i].da>astat[i].a) astat[i].modif=0;
 }
 if (config.whole) { astat[i].da=0; astat[i].a=config.maxx-1; }
 if (astat[i].modif)
 {
  #ifdef X11
  l=0;
  str=0; // make gcc HAPPY

  int xx,yy;
  char xstr[MAXVIDX+1];
  int cgc,len;

  if (zedmapped && (astat[i].modif==2 || !querykey()))
  {
   yy=i*MyWinInfo.fheight+MyWinInfo.fascent;
   xx=astat[i].da*MyWinInfo.fwidth;

   xstr[1]='\0';

   cgc=vbuf[l2+astat[i].da]&0xff00; len=0;

   for (j=astat[i].da; j<=astat[i].a; j++)
   {
    int dow=0,next=-1;

    xstr[len++]=vbuf[l2+j]&0xff;

    if (astat[i].a==j) dow=1; else
    { next=vbuf[l2+j+1]; if (cgc!=(next&0xff00)) dow=1; }
    if (dow)
    {
     if ((cgc&0x4000)==0)
     {
      XDrawImageString(display,zed_win,gcontext[cgc>>8],xx,yy,xstr,len);
      xx+=MyWinInfo.fwidth*len;
      len=0;
     }
     else
     {
      int k;
      for (k=0; k<len; k++)
      {
       int y2=yy-MyWinInfo.fascent;
       GC gt=gcontext[(cgc>>8)&0x3f];
       int dx=MyWinInfo.fwidth;
       int dy=MyWinInfo.fheight;

       XClearArea(display,zed_win,xx,y2,dx,dy,False);
       if (dx>=6 && dy>=6)
       {
        switch(xstr[k])
        {
         case 1:
          XDrawRectangle(display,zed_win,gt,xx+2,y2+2,dx-3,   1);
          XDrawRectangle(display,zed_win,gt,xx+2,y2+2,   1,dy-3);
         break;
         case 2:
          XDrawRectangle(display,zed_win,gt,xx  ,y2+2,dx-3,   1);
          XDrawRectangle(display,zed_win,gt,xx+dx-4,y2+2,   1,dy-3);
         break;
         case 3:
          XDrawRectangle(display,zed_win,gt,xx+2,y2+dy-4,dx-3,   1);
          XDrawRectangle(display,zed_win,gt,xx+2,y2,   1,dy-3);
         break;
         case 4:
          XDrawRectangle(display,zed_win,gt,xx  ,y2+dy-4,dx-3,   1);
          XDrawRectangle(display,zed_win,gt,xx+dx-4,y2,   1,dy-3);
         break;
         case 5:
          XDrawRectangle(display,zed_win,gt,xx+2,y2,   1,dy-1);
         break;
         case 6:
          XDrawRectangle(display,zed_win,gt,xx+dx-4,y2,   1,dy-1);
         break;
         case 7:
          XDrawRectangle(display,zed_win,gt,xx  ,y2+2,dx-1,   1);
         break;
         case 8:
          XDrawRectangle(display,zed_win,gt,xx  ,y2+dy-4,dx-1,   1);
         break;

         default:
          XFillRectangle(display,zed_win,gt,xx,y2,dx,dy);
         break;
        }
       } else XFillRectangle(display,zed_win,gt,xx,y2,dx,dy);

       xx+=MyWinInfo.fwidth;
      }
      len=0;
     }
    }
    if (next!=-1) cgc=next&0xff00;

    avideo[l2+j]=vbuf[l2+j]; astat[i].da++;
   }
   if (astat[i].da>astat[i].a) astat[i].modif=0;
  }
  #else

  if (i==config.maxy-1)
  {
   if (astat[i].a==config.maxx-1) astat[i].a--;
   if (astat[i].da>astat[i].a) astat[i].da--;
  }

  sprintf(str+l,"\x1b[%i;%iH",i+1,astat[i].da+1);
  l+=strlen(str+l);

  l2=i*config.maxx;
  for (j=astat[i].da; j<=astat[i].a; j++)
  {
   int attr;

   attr=((*(vbuf+l2+j))&0xff00)>>8;
   if (attr==0) attr=-config.col[4];
   if (attr==0x11) attr=-config.col[9];
   if (config.ansi!=3 && attr<0) attr=-attr;

   if (attr!=pattr || pattr==1000)
   {
    str[l++]=0x1b; str[l++]='['; str[l++]='0'; str[l++]=';';

    if (pattr<0 && attr>0)
    {
     str[l++]='1'; str[l++]='0'; str[l++]=';';
    }
    else
    if (pattr>0 && attr<0)
    {
     str[l++]='1'; str[l++]='1'; str[l++]=';';
    }

    pattr=attr;

    if (attr<0) attr=-attr;
    if (config.ansi==1 || config.ansi==3) // color ansi ?
    { // color
     if (attr&0x08) { str[l++]='1'; str[l++]=';'; }
     str[l++]='3';
     switch(attr&0x7)
     {
      case 0 : str[l]='0'; break;
      case 1 : str[l]='4'; break;
      case 2 : str[l]='2'; break;
      case 3 : str[l]='6'; break;
      case 4 : str[l]='1'; break;
      case 5 : str[l]='5'; break;
      case 6 : str[l]='3'; break;
      case 7 : str[l]='7'; break;
     }
     l++; str[l++]=';'; str[l++]='4';
     switch(attr&0x70)
     {
      case 0x00 : str[l]='0'; break;
      case 0x10 : str[l]='4'; break;
      case 0x20 : str[l]='2'; break;
      case 0x30 : str[l]='6'; break;
      case 0x40 : str[l]='1'; break;
      case 0x50 : str[l]='5'; break;
      case 0x60 : str[l]='3'; break;
      case 0x70 : str[l]='7'; break;
     }
     l++;
    }
    else
    { // monocrome
     if (attr&0x01) { str[l++]='1'; str[l++]=';'; }
     if (attr&0x02) { str[l++]='2'; str[l++]=';'; }
     if (attr&0x04) { str[l++]='4'; str[l++]=';'; }
     if (attr&0x08) { str[l++]='5'; str[l++]=';'; }
     if (attr&0x10) { str[l++]='7'; str[l++]=';'; }
     l--;
    }
    str[l++]='m';
   }
   str[l++]=(*(vbuf+l2+j))&0xff;
   avideo[l2+j]=vbuf[l2+j]; astat[i].da++;
   if (l>80)
   {
    str[l]='\0'; outansi(str); l=0;
    if (querykey()) { j=config.maxx+10; return(1); }
   }
  }
  if (astat[i].da>astat[i].a) astat[i].modif=0;
  #endif
 }
 return(0);
}

/***************************************************************************/
// forza ok la riga di stato (ansi mode)

void okstatus(int i)
{
 if (config.ansi>0 && config.ansi<128)
 {
  int l;
  char str[256];

  if (pattr<0 && config.ansi>0 && config.ansi<128) output("\x1b[10m");
  l=0; pattr=1000;

  goline(l,str,i);

  if (l==0) vsetcur(crx,cry);
  if (l>0) { str[l]='\0'; outansi(str); vsetcur(crx,cry); }
 }
 #ifdef MSDOS
 else if (config.ansi==128)
 {
  int l,da;

  l=config.maxx*i;
  da=astat[i].da;

  if (astat[i].modif)
  {
   movedata(vidmyds,(unsigned int)&vbuf[l+da],vidsel,vidoff+(l+da)*2,(astat[i].a-da+1)*2);
   astat[i].modif=0;
  }
 }
 #endif
 #ifdef X11
 XFlush(display);
 #endif
}

/***************************************************************************/

void refresh(void)
{
 if (config.ansi>0 && config.ansi<128)
 {
  #ifndef X11
  int i,l;
  char str[120];

  if (pattr<0 && config.ansi>0 && config.ansi<128) output("\x1b[10m");
  l=0; pattr=1000;

  if (!querykey())
  {
   for (i=0; i<config.maxy; i++)
    if (goline(l,str,i)) i=config.maxy+10;
   if (l==0) vsetcur(crx,cry);
  }
  if (l>0) { str[l]='\0'; outansi(str); vsetcur(crx,cry); }
  #else
  int i,l=0;

  for (i=0; i<config.maxy; i++) goline(l,0,i);
  vsetcur(crx,cry);
  #endif
 }
 #ifdef MSDOS
 else if (config.ansi==128)
 {
  int l,da;

  l=0;

  if (!querykey())
  {
   for (int i=0; i<config.maxy; i++)
   {
    da=astat[i].da;
    if (astat[i].modif)
    {
     movedata(vidmyds,(unsigned int)&vbuf[l+da],vidsel,vidoff+(l+da)*2,(astat[i].a-da+1)*2);
     astat[i].modif=0;
    }
    l+=config.maxx;
   }
  }
 }
 #endif
}

/***************************************************************************/
// ret speciali:
// ffffffff abortire il dialog
// fffffffe evento di X : uscita nulla
// fffffffd esecuzione comando interno
// fffffffc eseguito il clipimport !

unsigned int gettst(void)
{
 unsigned int ret=0;

 if (mouse.exec==-1)
 {
  do
  {
   refresh();
   ret=gettst2();
  } while (ret==0xfffffffe || ret==0xfffffffc);
  // ciclo nel caso sia un evento di X11
 } else ret=0xfffffffd;
 return(ret);
}

/***************************************************************************/
//    if ((rr&0xff00)==0) rr+=0x100; // keypad number

unsigned int gettst2(void)
{
 unsigned int ret=0;

 #ifdef MSDOS
  if (config.ansi>0 && config.ansi<128)
  {
   REGS regs;
   regs.h.ah=0x08;
   intdos(&regs,&regs);
   ret=regs.h.al;
  }
  else
  {
   int rr,s;

   _go32_dpmi_registers regs;
   regs.h.ah=0x10;
   regs.x.ss=regs.x.sp=0;
   _go32_dpmi_simulate_int(0x16,&regs);
   rr=regs.x.ax&0xffff;

   if ((rr&0xff)!=0 && (rr&0xff)!=0xe0) rr&=0xff;

   s=_farpeekb(vidsel,0x417);

   if (s&3 && (rr>255 || rr<32)) rr|=0x10000; // shift
   if (s&4 && rr>31) rr|=0x20000; // control
   if (s&8) rr|=0x40000; // alt

   ret=rr;
  }

 #elif defined X11

  ret=getxkey();

 #else
  fd_set fds;
  timeval tv;
  do
  {
   int fdmax=0;
   int i;

   FD_ZERO(&fds);
   FD_SET(0,&fds);
   if (pipes)
   {
    editor *p;
    for (i=0; i<edt.rtot(); i++)
    {
     p=*(editor **)edt.rdati();
     if (p->pipefd) { FD_SET(p->pipefd,&fds); fdmax=fdmax>?p->pipefd; }
     edt.next();
    }
   }

   int tvmin=-1;
   int sret;

   if (config.autosave)
   {
    int i,t;
    editor *myed;

    t=edt.rtot(); // numero di editor
    edt.nseek(0);
    for (i=0; i<t; i++)
    {
     myed=(*((editor **)edt.rdati()));
     if (myed->timeouttosave>0)
     {
      if (tvmin==-1) tvmin=myed->timeouttosave;
      else tvmin=tvmin<?myed->timeouttosave;
     }
     edt.next();
    }
   }

   if (tvmin>-1)
   {
    gettimeofday(&tv,(struct timezone *)0);

    tv.tv_sec=tvmin-tv.tv_sec+1>?0;
    tv.tv_usec=0;

    #ifdef __hpux
     sret=select(fdmax+1,(int *)&fds,0,0,&tv);
    #else
     sret=select(fdmax+1,&fds,0,0,&tv);
    #endif
   }
   else
   {
    #ifdef __hpux
     sret=select(fdmax+1,(int *)&fds,0,0,0);
    #else
     sret=select(fdmax+1,&fds,0,0,0);
    #endif
   }

   if (sret==0)
   {
    int i,t;

    gettimeofday(&tv,(struct timezone *)0);

    t=edt.rtot(); // numero di editor
    edt.nseek(0);
    for (i=0; i<t; i++)
    {
     editor *myed=(*((editor **)edt.rdati()));

     if (myed->timeouttosave>-1 &&
         (int)myed->timeouttosave<(int)tv.tv_sec &&
         myed->modif())
     {
      myed->savefile();
      for (i=0; i<winn; i++)
      {
       if (win[i].edtr==myed)
       {
        win[i].edtr->puthdr(1|2|4);
        okstatus(win[i].myda);
        i=winn;
       }
      }
     }
     edt.next();
    }
   }

   if (pipes)
   {
    editor *p;
    for (i=0; i<edt.rtot(); i++)
    {
     p=*(editor **)edt.rdati();
     if (p->pipefd) if (FD_ISSET(p->pipefd,&fds)) gopipe(p);
     edt.next();
    }
   }
  } while (!FD_ISSET(0,&fds));
  if (dialof&512) return(0xffffffff);

  unsigned char ch;

  if (read(0,&ch,1)==-1) return(0xffffffff); else ret=(unsigned char)ch;

  #if defined TIOCLINUX // shift status versione LINUX, se definita !
   int s=6;

   if (config.shiftlinux && ioctl(0,TIOCLINUX,&s)==0)
   {
    if (s&1 && (ret<32 || ret>126))  ret|=0x100; // shift
    if (s&4)  ret|=0x200; // control
    if (s&10) ret|=0x400; // i due alt
   }
  #endif
 #endif

 #ifdef SUPERDEBUG
 {
  int i;

  i=open("/tmp/savekeys",O_CREAT|O_WRONLY|O_APPEND);
  if (i>-1)
  {
   write(i,&ret,2);
   close(i);
  }
 }
 #endif

 return(ret);
}

/***************************************************************************/

void gettst3(void)
{
 #ifdef MSDOS
  if (config.ansi>0 && config.ansi<128)
  {
   REGS regs;
   regs.h.ah=0x08;
   intdos(&regs,&regs);
  }
  else
  {
   _go32_dpmi_registers regs;
   regs.h.ah=0x10;
   regs.x.ss=regs.x.sp=0;
   _go32_dpmi_simulate_int(0x16,&regs);
  }
 #else
  unsigned char ch;

  read(0,&ch,1);
 #endif
}

/***************************************************************************/
// controllo se c'e' un tasto che aspetta
// 0=nessuno !0=c'e'

int querykey(void)
{
 int ret=0;

 if (stopupdate) return(1);

 #ifdef MSDOS
  if (config.ansi>0 && config.ansi<128)
  {
   REGS regs;
   regs.h.ah=0x0b;
   intdos(&regs,&regs);
   ret=regs.h.al&1;
  }
  else
  {
   _go32_dpmi_registers regs;
   regs.h.ah=0x11;
   regs.x.ss=regs.x.sp=0;
   _go32_dpmi_simulate_int(0x16,&regs);
   ret=!(regs.x.flags&64);
  }
 #else
  fd_set fds;
  struct timeval tv;
  int fdmax;

  FD_ZERO(&fds);

  #ifdef X11
   fdmax=fd_x;
  #else
   fdmax=0;
  #endif

  FD_SET(fdmax,&fds);

  tv.tv_sec=0;
  tv.tv_usec=0;

  #ifdef __hpux
   select(fdmax+1,(int *)&fds,0,0,&tv);
  #else
   select(fdmax+1,&fds,0,0,&tv);
  #endif

  ret=FD_ISSET(fdmax,&fds);

 #endif
 return(ret);
}

/***************************************************************************/

int chksh(int ch)
{
 if (ch=='\xff') return(0); // caso di sequenza annullata

 int r;

 r=1;
 if (ch)
 {
  // flags operation
  if (ch&(256|512)) // primo set
  {
   if ( (config.vars&1) && ch&256) r=0;
   else
   if (!(config.vars&1) && ch&512) r=0;
  }
  if (ch&(1024|2048)) // secondo set
  {
   if ( (config.vars&2) && ch&1024) r=0;
   else
   if (!(config.vars&2) && ch&2048) r=0;
  }
  if (ch&(4096|8192)) // terzo set
  {
   if ( (config.vars&4) && ch&4096) r=0;
   else
   if (!(config.vars&4) && ch&8192) r=0;
  }
  if (ch&(16384|32768)) // terzo set
  {
   if ( (config.vars&8) && ch&16384) r=0;
   else
   if (!(config.vars&8) && ch&32768) r=0;
  }

  #if defined UNIX && !defined X11 // timed operation
   if (ch&64) // ci sono altri caratteri in attesa ?
   {
    struct timeval wt;
    fd_set wfd;

    wt.tv_sec=config.delay/1000;
    wt.tv_usec=(config.delay%1000)*1000;

    FD_ZERO(&wfd);
    FD_SET(0,&wfd);

    #ifdef __hpux
     select(1,(int *)&wfd,0,0,&wt);
    #else
     select(1,&wfd,0,0,&wt);
    #endif
    if (FD_ISSET(0,&wfd)) r=0;
   }
  #endif
 }

 return(r);
}

/***************************************************************************/
// flag: 0=read, 1=write_create_trunc, 2=write
// ret: 0=ok, -1=error, -2=not found >0 handler

int my_open(char *nome,int flag)
{
  #ifndef UNIX
   int ifl=0;
  #endif
  int r=0;

 switch(flag)
 {
  #ifndef UNIX
   case 0 : ifl=O_RDONLY|O_BINARY; break;
   case 1 : ifl=O_WRONLY|O_BINARY|O_CREAT|O_TRUNC; break;
   case 2 : ifl=O_WRONLY|O_BINARY|O_CREAT; break;
  #else
   case 0 : r=open(nome,O_RDONLY); break;
   case 1 : r=creat(nome,0666); break;
   case 2 : r=open(nome,O_WRONLY|O_CREAT,0666); break;
  #endif
 }

 #ifndef UNIX
  r=open(nome,ifl,S_IREAD|S_IWRITE);
 #endif
 if (r==-1 && errno==ENOENT) r=-2;
 return(r);
}

/***************************************************************************/

int my_access(char *nome)
{
 return(access(nome,R_OK));
}

/***************************************************************************/
/***************************************************************************/
// da qui in poi tutto portable
/***************************************************************************/
/***************************************************************************/

svsave *vsave(int yda,int ya)
{
 int d;
 svsave *ss=new svsave;

 if (!ss) return(0);
 ss->ya=ya; ss->yda=yda; ss->xmax=config.maxx;
 d=config.maxx*(ya-yda+1)*2;
 ss->buf=new char [d];
 if (!ss->buf) { delete ss; return(0); }

 memcpy(ss->buf,vbuf+yda*config.maxx,d);
 return(ss);
}

/***************************************************************************/

void vrest(svsave *ss)
{
 if (ss)
 {
  if (ss->yda<config.maxy)
  {
   int y;

   for (y=ss->yda; y<=(config.maxy<?(ss->ya)); y++)
    memcpy(vbuf+y*config.maxx,ss->buf+((y-(ss->yda))*ss->xmax)*2,
           (ss->xmax)*2<?config.maxx*2);
  }
  if (config.ansi) astatok(ss->yda,ss->ya,0,config.maxx-1);
  delete ss->buf; delete ss;
 }
// refresh();
}

/***************************************************************************/
// ret: uno dei keys o 255 se esc

char confirm(char *sstr,char *keys,int yy,short int col)
{
 char *save;
 int d,ret;
 int cx,cy;

 save=new char [config.maxx*2+2];
 saveline(yy,save);
 putchn(' ',31,0,yy,col);
 putstr(sstr,0,0,yy,col);
 vsavecur(&cx,&cy); vsetcur(strlen(sstr),yy);
 do
 {
  ret=0;
  d=0;
  switch(exectst(&d))
  {
   case 1 : if (keys) if (strchr(keys,d)) ret=tolower(d); break;
   case 2 : if (d==12) ret=255; break; // se esc, abort, torno 255
  }
 } while (!ret);
 restline(yy,save); vrestcur(cx,cy);

 delete save;
 return(ret);
}

/***************************************************************************/
// salva la linea yy in save

void saveline(int yy,char *save)
{
 short int *start;
 start=vbuf+yy*config.maxx; memcpy(save,start,config.maxx*2);
}

/***************************************************************************/
// recupera la linea yy in save

void restline(int yy,char *save)
{
 short int *start;
 start=vbuf+yy*config.maxx; memcpy(start,save,config.maxx*2);
 if (config.ansi) astatok(yy,yy,0,config.maxx-1);
// refresh();
}

/***************************************************************************/
// cancella lo schermo

void clearscr(void )
{
 short int *start,col2;
 int i,fi;
 short int *startv/*,colv*/;

 start=vbuf; startv=avideo;
 fi=config.maxx*config.maxy;
 col2=(short int)(config.col[1]<<(short int)8)|(short int)' ';
 #ifndef X11
  if (config.ansi>0 && config.ansi<128)
  {
   outansi("\x1b[0;10m\x1b[2J"); // ansi
  }
 #endif
 if (config.ansi)
 { for (i=0; i<fi; i++) { *(start++)=col2; *(startv++)=-1/*colv*/; } }
 else
 { for (i=0; i<fi; i++) *(start++)=col2; }
 #ifdef MSDOS
 if (config.ansi==128)
 {
  movedata(vidmyds,(unsigned int)&vbuf[0],vidsel,vidoff,fi*2);
 }
 #endif
 vsetcur(0,0);
}

/***************************************************************************/
// scrive caratteri

void putchn(char ch,int fi,int x,int y,short int col)
{
 short int *start,col2;
 int i;

 start=vbuf+y*config.maxx+x;
 col2=(short int)((short int)col<<(short int)8)|((short int)(unsigned char)ch);
 if (x+fi>config.maxx) fi=config.maxx-x;
 for (i=0; i<fi; i++) *(start++)=col2;

 if (config.ansi && fi>0) astatok(y,y,x,x+fi-1);
}

/***************************************************************************/
// scrive string normale ASCIIZ

void putstr(char *str2,int fi,int x,int y,short int col)
{
 short int *start,col2;
 int i;
 unsigned char *str;

 start=vbuf+y*config.maxx+x;
 str=(unsigned char *)str2;
 col2=(short int)((short int)col<<(short int)8);
 if (fi==0) fi=strlen((char *)str);
 if (x+fi>config.maxx) fi=config.maxx-x;
 for (i=0; i<fi; i++) *(start++)=col2|(short int)*(str++);

 if (config.ansi && fi>0) astatok(y,y,x,x+fi-1);
}

/***************************************************************************/
// scrive string da inizio video, space fill, con lunghezza

void vputstr(char *str2,int fi,int y,short int col)
{
 short int *start,col2;
 int i;
 unsigned char *str;

 start=vbuf+y*config.maxx;

 str=(unsigned char *)str2;
 col2=(short int)((short int)col<<(short int)8);
 if (fi>config.maxx) fi=config.maxx;
 for (i=0; i<fi; i++) *(start++)=col2|(short int)*(str++);
 for (; i<config.maxx; i++) *(start++)=col2|(short int)' ';

 if (config.ansi) astatok(y,y,0,config.maxx-1);
}

/***************************************************************************/
// scrive integer value

void putint(int ii,int n,int x,int y,short int col)
{
 int i,fi;
 unsigned char str2[30],*str;

 str=str2;
 for (i=0; i<n; i++) str[i]=' ';
 sprintf((char *)str,"%i",ii);
 str[strlen((char *)str)]=' '; str[n]='\0';

 short int *start,col2;
 start=vbuf+y*config.maxx+x;

 col2=(short int)((short int)col<<(short int)8);
 fi=strlen((char *)str);
 if (x+fi>config.maxx) fi=config.maxx-x;
 for (i=0; i<fi; i++) *(start++)=col2|(short int)*(str++);

 if (config.ansi && fi>0) astatok(y,y,x,x+fi-1);
}

/***************************************************************************/
// scrive carattere singolo

void putchr(unsigned char chr,int x,int y,short int col)
{
 vbuf[y*config.maxx+x]=(short int)(((short int)col<<(short int)8)|(short int)chr);
 if (config.ansi) astatok(y,y,x,x);
}

/***************************************************************************/
// scrolla una parte di video
// yd deve distare da yda di UNA riga !

void scroll(int yda,int ya,int yd)
{
 memmove(vbuf+yd*config.maxx,vbuf+yda*config.maxx,(ya-yda+1)*config.maxx*2);

 #ifndef X11
  if (config.ansi==3) // HEHE ho lo scroll !
  {
   char str[80];

   memmove(avideo+yd*config.maxx,avideo+yda*config.maxx,(ya-yda+1)*config.maxx*2);
   memmove(&astat[yd],&astat[yda],(ya-yda+1)*sizeof(_modif));

   if (yd>yda)
   {
    sprintf(str,"\x1b[0m\x1b[%i;%ir\x1b[%i;1H\x1b\x4d",yda+1,ya+2,yda+1);
    outansi(str);
    astat[yda].da=0; astat[yda].a=config.maxx-1; astat[yda].modif=2;
   }

   if (yd<yda)
   {
    sprintf(str,"\x1b[0m\x1b[%i;%ir\x1b[%i;1H\x1b\x44",yda,ya+1,ya+1);
    outansi(str);
    astat[ya].da=0; astat[ya].a=config.maxx-1; astat[ya].modif=2;
   }
   sprintf(str,"\x1b[0;0r"); outansi(str);

   vsetcur(0,0,1);
  }
/*  #ifdef MSDOS // movedata non funziona all'indietro !!!
  else if (config.ansi==128)
  {
   movedata(vidsel,vidoff+yda*config.maxx*2,
            vidsel,vidoff+yd*config.maxx*2,(ya-yda+1)*config.maxx*2);

//   memmove(avideo+yd*config.maxx,avideo+yda*config.maxx,(ya-yda+1)*config.maxx*2);

   memmove(&astat[yd],&astat[yda],(ya-yda+1)*sizeof(_modif));
  }
  #endif*/
  else
  {
   astatok(yda,ya,0,config.maxx-1); // l'ANSI non ha uno scroll !
   astatok(yd,yd+(ya-yda),0,config.maxx-1);
  }
 #else
  if (zedexpose)
  {
   vsetcur(0,0,2); // cancello cursore
   memmove(avideo+yd*config.maxx,avideo+yda*config.maxx,(ya-yda+1)*config.maxx*2);
   memmove(&astat[yd],&astat[yda],(ya-yda+1)*sizeof(_modif));

   XCopyArea(display,zed_win,zed_win,gcursor,
             0,yda*MyWinInfo.fheight,
             config.maxx*MyWinInfo.fwidth,MyWinInfo.fheight*(ya-yda+1),
             0,yd*MyWinInfo.fheight);
   vsetcur(0,0,1); // riscrive cursore
  }
  else
  {
   astatok(yda,ya,0,config.maxx-1); // scroll manuale !
   astatok(yd,yd+(ya-yda),0,config.maxx-1);
  }
 #endif
}

/***************************************************************************/
// controlla nella tabella dei comandi e dei tasti i codici in arrivo
// rets : 1 : devi fare una execch
//        2 : devi fare una execcmd e ti becchi il suo exit, se ==0 cicla
//        3 : devi ciclare

int exectst(int *dato)
{
 static unsigned int abtkey=0;      // tasto eliminato per recupero
 static unsigned int abtkeyaval=0;  // c'e' il tasto ?
 static unsigned int seq[MAXXKEYS];
 static unsigned int l=0;
 static int lstr=0;
 static unsigned char *sp=0;  // punta ai comandi da eseguire
 static unsigned char *ssp=0; // comandi da eseguire alla ricezione di ENTER
 static int ignore=0;

 unsigned int ch=0;
 int ok=0;
 unsigned int *p;

 /* casi particolari di dato
    dato==0 ignore==1 (ignora gli enter e gli esc)
    dato==1 ignore==0 (prendi gli enter e gli esc)
    dato==2 elimina ssp e ignore=0;
    dato==3 obbliga ssp se ssp e ignore=0;
    dato==4 elimina sp e ssp, lstr
    *dato positivo setta sp=dato...
 */

 switch((int)dato)
 {
  case 0 : ignore=0; return(0);
  case 1 : ignore=1; return(0);
  case 2 : ssp=0; ignore=0; return(0);
  case 3 : if (ssp) { sp=ssp; ssp=0; ignore=0; } return(0);
  case 4 : sp=0; ssp=0; lstr=0; return(0);
 }

 if (((unsigned int)dato)>10 && (*dato)>0)
 {
  sp=config.cmds+(*dato-1); lstr=0; ssp=0;
  return(0);
 }
 // per comandi del menu

 #ifdef X11
  if (mouse.togox>-1 || mouse.togoy>-1) okmouse();
 #endif

 if (mouse.exec!=-1) { *dato=mouse.exec; mouse.exec=-1; return(2); }

 if (sp && !lstr) if (*sp==100) { ssp=sp+1; sp=0; return(3); }

 if (!sp) // non c'e' comando da eseguire
 {
  p=config.keys; ok=1;

  if (abtkeyaval) { ch=abtkey; abtkeyaval=0; } else ch=gettst();

  if (ch==0xffffffff) { *dato=12; return(2); } // caso abort da dialog

  if (l==0)
  {
   if (ch<32 || ch>126 || !chksh(1+4+16))
   // le sequenze cominciano con un numero <32 o >126
   {
    while (*p)
    {
     if (p[0]==1 && p[3]==ch && chksh(p[1]))
     { sp=config.cmds+p[2]; ok=0; break; }
     else if (p[0]>1 && p[3]==ch ) { seq[l++]=ch; ok=0; break; }
     p+=p[0]+3; // +3:lung+shift+punt
    }
   }
  }
  else
  {
   int okk;

   okk=1; seq[l++]=ch; ok=0; // aggiorno la sequenza
   while (*p)
   {
    if (p[0]==l && !memcmp(&p[3],seq,l*4) && chksh(p[1]))
    { sp=config.cmds+p[2]; break; }
    else
    if (p[0]>l && !memcmp(&p[3],seq,l*4))
    { okk=0; break; } // esiste una possibile
    p+=p[0]+3;
   }
   if (okk)
   {
    l=0; ok=0;
    if (!sp) { abtkey=ch; abtkeyaval=1; } // caso sequenza orrida
   } // se sequenza orrida o trovata,canc all e non exec
  }

  if (sp) // sistemo l'inizio dell'esecuzione
  {
   if (*sp==0xff) { sp++; lstr=*(sp++); } // e' una stringa ?
   l=0;
  }
 }

 if (sp) // comando da eseguire ?
 {
  if (lstr)// e' una stringa ?
  { *dato=*(sp++); lstr--; addmacro(0,(char)*dato); return(1); }
  else
  if (*sp) // se c'e' ancora un comando da fare
  {
   if (*sp==0xff) // e' una stringa ?
   {
    sp++; lstr=*(sp++); *dato=*(sp++); lstr--; addmacro(0,(char)*dato);
    return(1);
   }
   else
   {
    *dato=*(sp++);
    if (ssp && !ignore)
     if (*ssp) if (*dato==136) { sp=ssp; ssp=0; } else if (*dato==12) ssp=0;

    addmacro((unsigned char)*dato,0);
    return(2);
   }
  }
  sp=0;
 }
 else
 if (ok)
 {
  #ifdef X11
   if (ch==XK_Tab) ch=9;
  #endif

  if (ch<255)
  {
   *dato=ch; addmacro(0,(char)ch);
   return(1);
  }
  else return(3);
 }

 return(3);
}

/***************************************************************************/

//char *ibuff;
//int ifile=0;
//int fleof;
//int blen,bpos;

/***************************************************************************/
/*
 ret:
 0 : ok
 1 : non c'e' il file
 2 : open error
 3 : no memoria
 */
int mopen(char *nome,mfile *mf)
{
 int err;

 mf->h=0; mf->buf=0;

 err=0;
 if ((mf->h=my_open(nome,0))<0)
 {
  if (mf->h==-2) err=1; else err=2;
  mf->h=0;
 }
 if (err==0) mf->buf=new char [BUFFSIZE];
 mf->len=0; mf->pos=BUFFSIZE; mf->eof=0;
 return(err);
}

/***************************************************************************/
/*
 ret:
 0 : ok
 1 : eof, nisba letto o errore
 se errore err=10;
 */
int mread(char **str,int maxlen,int *error,int *len,mfile *mf)
{
 if ((mf->len==0 && mf->eof) || *error) return(1); // fine del file
 if (mf->len<maxlen && !mf->eof) // devo fillare il buffer
 {
  int r;

  if (mf->buf==0) return(1); //sanity check
  memcpy(mf->buf,mf->buf+mf->pos,mf->len);
  mf->pos=0;
  r=read(mf->h,mf->buf+mf->len,BUFFSIZE-mf->len);
  if (r==-1) { *error=10; return(1); }
  if (r==0) { mf->eof=1; if (mf->len==0) return(1); } else mf->len+=r;
 }

 {
  if (maxlen>mf->len) maxlen=mf->len;

  unsigned int l;
  char *p,*p1,*p2;

  p=mf->buf+mf->pos; l=0;

  p1=(char *)memchr((void *)p,'\x0d',maxlen);
  p2=(char *)memchr((void *)p,'\x0a',maxlen);

  if (p1==0 || (p1>p2 && p2!=0)) p1=p2;
  if (p1) l=(unsigned int)(p1-p); else l=maxlen;

  if (*str) memcpy(*str,p,l); else *str=p;
  mf->len-=l; mf->pos+=l;

  if (p1)
  {
   if (mf->len>1 && ((*p1=='\x0d' && *(p1+1)=='\x0a'))) { mf->len-=2; mf->pos+=2; }
   else
   if (mf->len>0 && (*p1=='\x0a' || *p1=='\x0d')) { mf->len--; mf->pos++; }
  }

  if (l>0 && *(p+(l-1))==config.softret) { l--; l|=0x4000; }

  *len=l;
 }
 return(0);
}

/***************************************************************************/

void mclose(mfile *mf)
{
 if (mf->h) close(mf->h);
 delete mf->buf; mf->h=0;
}

/***************************************************************************/

void output(char *str)
{
 write(1,str,strlen(str));
}

/***************************************************************************/

//inline void outansi(char *str)
//{
// output(str);
//}

/***************************************************************************/

void outint(int i)
{
 char str[20];
 sprintf(str,"%i",i);
 write(1,str,strlen(str));
}

/***************************************************************************/

char itoh(int i)
{
 if (i<10) return(i+'0'); else return((i-10)+'A');
}

/***************************************************************************/

void astatok(int yda,int ya,int xda,int xa)
{
 int y;

 for (y=yda; y<=ya; y++)
 {
  if (astat[y].modif)
  { astat[y].da=astat[y].da<?xda; astat[y].a=astat[y].a>?xa; }
  else { astat[y].modif=1; astat[y].da=xda; astat[y].a=xa; }
 }
}

/***************************************************************************/

static unsigned int *macroseq=0;
static int macroseql=0;
static int macrostr=0;

/***************************************************************************/

void startmacro(int yy)
{
 char *save;
 int err;
 char *sstr="Select macro sequence, ^M ends";
 int cx,cy;

 save=new char[config.maxx*2+2];
 macroseq=new unsigned int [MAXXKEYS];
 macrocmd=new char [1024]; macropos=macrostr=0; // -1=empty

 err=0;

 if (macroseq && macrocmd)
 {
  saveline(yy,save);
  putchn(' ',31,0,yy,config.col[4]);
  putstr(sstr,0,0,yy,config.col[4]);
  vsavecur(&cx,&cy); vsetcur(strlen(sstr),yy);

  macroseql=0;

  int ch;
  unsigned int *pnt=0;
  do
  {
   // in macroseql ho il prossimo byte libero
   ch=gettst(); // becco il carattere
   #ifdef X11
    if (ch!=XK_Return)
   #else
    if (ch!=13)
   #endif
   {
    macroseq[macroseql++]=ch; // lo scrivo

    int chk=0;      // parte la verifica
    unsigned int *kchk=config.keys;

    while (chk<config.pkeys && err==0)
    {
     int l;
     l=kchk[0];        // l==lunghezza macro in memoria
     if (l==macroseql) // macroseql==lunghezza macro in registrazione
     {
      int d;

//      d=0; // d=1 se diversi

      d=(memcmp(&kchk[3],macroseq,l*4)!=0);
//      for (i=0; i<l; i++)
//       if ((*(pchk+1+i))!=(unsigned char)macroseq[i]) { d=1; break; }

      if (d==0) if (chksh(kchk[1]&64))
      { err=2; pnt=kchk; } // se uguali

//      if (d==0) { err=2; pnt=pchk; } // se uguali
     }
     chk+=kchk[0]+3; kchk+=kchk[0]+3;
    }
   }
  }
  #ifdef X11
   while (ch!=XK_Return && macroseql<119 && err==0);
  #else
   while (ch!=13 && macroseql<119 && err==0);
  #endif

  if (macroseql>118)
  {
   vrestcur(cx,cy); confirm("Sequence too long ! hit ESC","",yy,config.col[5]);
   err=1;
  }
  else
  if (err==2)
  {
   int ret;
   vrestcur(cx,cy);
   ret=confirm("Erase existing Sequence (Y/N/Esc) ?","yn",yy,config.col[5]);

   if (ret=='y')
   {
    pnt[1]=0xffffffff; memset(&pnt[3],255,pnt[0]*4);
    // fillo con 255==sequenza annullata
    err=0; macropos=0; macrostr=0;
   }
  }
  if (macroseql==0) err=4;

  // verifica post-inserimento

  // devo verificare che non ci sia una sequenza piu' lunga
  // e con inizio uguale a quella data
  // oppure

  if (err==0)
  {
   int chk=0;      // parte la verifica
   unsigned int *kchk=config.keys;

   while (chk<config.pkeys && err==0)
   {
    int l;
    l=kchk[0];
    if (l>macroseql)
    {
     int d;

//     d=0; // d=1 se diversi
     d=(memcmp(&kchk[3],macroseq,macroseql*4)!=0);

//     for (i=0; i<macroseql; i++)
//      if ((*(pchk+1+i))!=(unsigned char)macroseq[i]) { d=1; break; }

     if (d==0) err=3;// se uguali
    }
    chk+=kchk[0]+3; kchk+=kchk[0]+3;
   }
  }
  if (err==3)
  {
   vrestcur(cx,cy);
   confirm("Sequence too short ! hit ESC","",yy,config.col[5]);
  }
  restline(yy,save); vrestcur(cx,cy);
 } else { confirm(s_oom,"",yy,config.col[5]); err=1; }

 if (err) abortmacro();
 delete save;
}

/***************************************************************************/

void addmacro(unsigned char cmd,char chr)
{
 if (cmd==192 || macrostop || macrocmd==0) return;

 if (cmd)
 {
  macrostr=0;
  macrocmd[macropos++]=cmd;
 }
 else
 if (macrostr)
 {
  unsigned char uc;

  uc=((unsigned char)macrocmd[macrostr]);
  uc++;
  macrocmd[macrostr]=(char)uc;

  if (((unsigned char)macrocmd[macrostr])>250)
  macrostr=0;
  macrocmd[macropos++]=chr;
 }
 else
 {
  macrocmd[macropos++]='\xff';
  macrostr=macropos; macrocmd[macropos++]='\1';
  macrocmd[macropos++]=chr;
 }

 if (macropos>1020)
 {
  confirm("Macro too big ! hit ESC","",0,config.col[5]);
  abortmacro();
 }
}

/***************************************************************************/

void abortmacro(void)
{
 delete macroseq; delete macrocmd;
 macroseq=0;
 macrocmd=0;
 macroseql=macropos=macrostr=0;
}

/***************************************************************************/

void recmacro(void)
{
 unsigned int opkeys,opcmds;
 unsigned int *nkeys;
 unsigned char *ncmds;

 macrocmd[macropos-1]='\0'; // l'ultimo comando e' il macro record, lo elimino
 opkeys=config.pkeys; opcmds=config.pcmds;
 config.pkeys+=macroseql+3; config.pcmds+=macropos;
 nkeys=(unsigned int *)realloc(config.keys,config.pkeys*4);
 ncmds=(unsigned char *)realloc(config.cmds,config.pcmds);
 if (nkeys && ncmds)
 { // ok to add command
  nkeys[opkeys-2]=macroseql;       // lunghezza
  nkeys[opkeys-1]=0;               // shift status
  nkeys[opkeys-0]=opcmds;          // puntatore ai comandi
  memcpy(nkeys+opkeys+1,macroseq,macroseql*4); // sequenza
  nkeys[opkeys+macroseql+1]=0;     // tappo
  nkeys[opkeys+macroseql+2]=0;     // tappo

  memcpy(ncmds+opcmds,macrocmd,macropos);
  config.keys=nkeys; config.cmds=ncmds;
 }
 else confirm(s_oom,"",0,config.col[5]);

 abortmacro();
}

/***************************************************************************/

void writemacro(int yy)
{
 char *save,*line;
 int err;
 char *sstr="Select macro sequence";
 unsigned int *mcroseq;
 unsigned char *mcrocmd;
 int mcropos,mcrostr,mcroseql;
 int cx,cy;

 vsavecur(&cx,&cy);
 save=new char[config.maxx*2+2];
 mcroseq=new unsigned int [MAXXKEYS];
 mcrocmd=new unsigned char [1024];
 mcropos=mcrostr=0; // -1=empty
 line=(char *)mcrocmd;

 err=0;

 if (mcroseq && mcrocmd)
 {
  saveline(yy,save);
  putchn(' ',31,0,yy,config.col[4]);
  putstr(sstr,0,0,yy,config.col[4]);
  vsetcur(strlen(sstr),yy);

  mcroseql=0;

  int ch;
  int forse=0,found=0;
  unsigned int *pnt=0;
  do
  {
   // in mcroseql ho il prossimo byte libero
   ch=gettst(); // becco il carattere
//   if (ch&0x200) { ch&=0xff; timet=1; }
   //if (ch!=13)
   {
    mcroseq[mcroseql++]=ch; // lo scrivo

    int chk=0;      // parte la verifica
    unsigned int *kchk=config.keys;

    forse=0; found=0;

    while (chk<config.pkeys && err==0)
    {
     int l;
     l=kchk[0];
     if (l>=mcroseql)
     {
      int d;

//      d=0; // d=1 se diversi
//      for (i=0; i<mcroseql; i++)
//       if ((*(pchk+1+i))!=(unsigned char)mcroseq[i]) { d=1; break; }

      d=(memcmp(&kchk[3],mcroseq,mcroseql*4)!=0);

      if (d==0 && l==mcroseql && chksh(kchk[1]))
      { found=1; pnt=kchk; }
      if (d==0) forse=1; // se inizio o tutta sequenza uguale
     }
     chk+=l+3; kchk+=l+3;
    }
   }
  }
  while (forse && !found);

  if (!found)
  {
   vrestcur(cx,cy); confirm("Sequence not found ! hit ESC","",yy,config.col[5]);
   err=1;
  }

  // devo scrivere la sequenza

  if (err==0)
  {
   unsigned int t;
   int l,s,i;
   char str[10];
   #ifdef MSDOS
    strcpy(line,"\x0d\x0akey ");
   #else
    strcpy(line,"\x0akey ");
   #endif

   for (i=0; i<(int)pnt[0]; i++)
   {
    l=0;
    t=pnt[i+3];
    if (t>32 && t<126)
    {
     if (t=='\\' || t=='&' || t==';') str[l++]='\\';
     str[l++]=t;
    }
    else
    {
     str[l++]='\\';

     if (t<256)
     {
      str[l++]='u';
      str[l++]=itoh((t&0x0000000f0)>>4);
      str[l++]=itoh((t&0x00000000f)>>0);
     }
     else if (t<65536)
     {
      str[l++]='w';
      str[l++]=itoh((t&0x00000f000)>>12);
      str[l++]=itoh((t&0x000000f00)>>8);
      str[l++]=itoh((t&0x0000000f0)>>4);
      str[l++]=itoh((t&0x00000000f)>>0);
     }
     else
     {
      str[l++]=itoh((t&0x0f0000000)>>28);
      str[l++]=itoh((t&0x00f000000)>>24);
      str[l++]=itoh((t&0x000f00000)>>20);
      str[l++]=itoh((t&0x0000f0000)>>16);
      str[l++]=itoh((t&0x00000f000)>>12);
      str[l++]=itoh((t&0x000000f00)>>8);
      str[l++]=itoh((t&0x0000000f0)>>4);
      str[l++]=itoh((t&0x00000000f)>>0);
     }
    }
    str[l]='\0';
    strcat(line,str);
   }
   s=pnt[1]; // shift status
   l=strlen(line);

   if (s)
   {
    line[l++]='&';
    if (s&64) line[l++]='t';
    if (s&256) line[l++]='a';
    if (s&512) line[l++]='A';
    if (s&1024) line[l++]='b';
    if (s&2048) line[l++]='B';
    if (s&4096) line[l++]='c';
    if (s&8192) line[l++]='C';
    if (s&16384) line[l++]='d';
    if (s&32768) line[l++]='D';
   }
   line[l++]=' '; line[l]='\0';

   int h;
   char nome[128];
   {
    dialog file;
    nome[0]=0;

    err=file.ask("File (append) ",0,nome);
   }

   //strcpy(nome,argv0); *(strrchr(nome,'.')+1)='\0'; strcat(nome,"cfg");

   if ((h=my_open(nome,2))<0) err=1;
   if (err==0)
   {
    lseek(h,0,SEEK_END);

    unsigned char *cmd=config.cmds+pnt[2];
    int lstr=0;

    while (*cmd || lstr)
    {
     if (l>70)
     {
      line[l++]='\\'; write(h,line,l); l=0;
      #ifdef MSDOS
       line[l++]='\x0d'; line[l++]='\x0a'; line[l++]=' ';
      #else
       line[l++]='\x0a'; line[l++]=' ';
      #endif
     }

     if (*cmd==255 || lstr)
     { // caso "stringa"
      int i;

      if (lstr==0) { lstr=*(cmd+1); cmd+=2; }
      line[l++]='"';
      for (i=0; lstr>0 && i<10; i++)
      {
       char ch;

       ch=*cmd;

       if (ch<32 || (unsigned char)ch>127)
       {
        line[l++]='\\';
        line[l++]=itoh((ch&0x0f0)>>4); line[l++]=itoh(ch&0xf);
       }
       else
       {
        if (ch=='\\' || ch==';' || ch=='"') line[l++]='\\';
        line[l++]=ch;
       }
       lstr--; cmd++;
      }
      line[l++]='"'; line[l++]=' ';
     }
     else
     {
      for (int tok=0; tabcom[tok].ncom<255; tok++)
      {
       if (*cmd==tabcom[tok].ncom)
       {
        strcpy(line+l,tabcom[tok].com); l=strlen(line); line[l++]=' '; break;
       }
      }
      if (lstr==0) cmd++;
     }
    }
    if (line[l]==' ') l--;
    #ifdef MSDOS
     line[l++]='\x0d'; line[l++]='\x0a';  line[l]='\0';
    #else
     line[l++]='\x0a';  line[l]='\0';
    #endif
    write(h,line,l);
    close(h);
   } // end no error
   else confirm("Error opening file ! hit ESC","",yy,config.col[5]);
  }

  restline(yy,save); vrestcur(cx,cy);
 } else { confirm(s_oom,"",yy,config.col[5]); err=1; }
 delete mcroseq; delete mcrocmd;

 delete save;
 vrestcur(cx,cy);
}

/***************************************************************************/
#ifdef UNIX

void strlwr(char *str)
{
 for (int i=0; str[i]!='\0'; i++) str[i]=tolower(str[i]);
}

#endif
/***************************************************************************/

int memicmp(char *p1,char *p2,int ln)
{
 char str1[1030],str2[1030];

 if (ln<1030)
 {
  memcpy(str1,p1,ln); memcpy(str2,p2,ln);
  strlwr(str1); strlwr(str2);
  return(memcmp(str1,str2,ln));
 }
 else { printf("Internal error 1!\n"); exit(1); }
}

/***************************************************************************/

