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

/* main.cc - gestione multi finestre/editor e ciclo centrale */

#include <sys/stat.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <regex.h>
#ifdef X11
 #include <X11/Xlib.h>
#endif

#include "zed.h"

#ifndef UNIX
 #include <process.h>
 #include <dos.h>
 #include <conio.h>
 #include <dpmi.h>
#else
 #include <fcntl.h>
 #include <sys/wait.h>
// #include <sys/time.h>
// #include <termios.h>
// #include <stdio.h>
#endif

/***************************************************************************/
// variabili locali

_wind *unzoom=0;
int unzoomc,unzoomn=0;
char *spath=0;
int sdisk=-1;

//void signals(int i);
//void sigusr1(int i);

/***************************************************************************/
// variabili globali

#ifdef MSDOS
 char *argv0;
#endif

char *s_str=0;         // stringa da cercare per "rifa'"
char *s_strto=0;       // stringa da sostituire per "rifa'"
char *s_stropti=0;     // stringa delle opzioni di ricerca
char *s_stropts=0;     // stringa delle opzioni di ricerca
char *s_stropta=0;     // stringa delle opzioni di ricerca
short int s_flag=0;    // flags della ricerca/sostituzione

_wind *win;
int cwin,winn,savecwin=-1;
list edt;
editor *nedt;
char *macrocmd=0;
int macropos=0;
int macrostop=0;
int pipes=0;
int dialof=0; // flags per signal WINCH & dialog
int stopupdate=0;
_mouse mouse={-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
selez *crtselez=0;
dialog *crtdialog=0;

#ifdef MSDOS
 int vidsel=0; // selettore per il segmento per l'accesso al video
 int vidoff=0; // offset lineare inizio buffer video
 int vidmyds=0; // risultato di _go32_my_ds;
#endif

/***************************************************************************/

void resetcfg(int flag,char *str)
{
 if (config.keys==(unsigned int *)0xffffffff) config.keys=0;
 if (config.strs==(char *)0xffffffff) config.strs=0;
 if (config.cmds==(unsigned char *)0xffffffff) config.cmds=0;
 if (config.mnus==(_menu *)0xffffffff) config.mnus=0;

 delete config.keys; delete config.cmds;
 delete config.strs; delete config.mnus;

 memset(&config,0xff,sizeof(_config));     // azzero la config
 config.keys=0; config.cmds=0; config.strs=0; config.mnus=0;
 config.pkeys=config.pcmds=config.pstrs=config.pmnus=0;

 output("\n  Error(s), ");
 if (flag>1) output("ignoring command line and ");
 output("reverting to ");
 output(str);
 output(" config.\n\n");
}

/***************************************************************************/

#ifdef UNIX

void signals(int sig)
{
 if (sig==SIGWINCH) { goresize(); signal(sig,signals); }
}

/***************************************************************************/

void goresize(void)
{
 int t;

 t=init(1);
 if (t!=-1)
 {
  chkwinsize(); // check video

  _wind *nwin;

  if (winn>0)
  {
   nwin=new _wind[config.maxy/3+1];
   memcpy(nwin,win,sizeof(_wind)*winn);
   delete win; win=nwin;
  }
  clearscr();

  if (winn>0) { wup(); win[cwin].edtr->setcur(); }

  if ((dialof&256)==256) dialof|=512; // esco dall'eventuale dialog

  dialof&=~65536;

  if (!(dialof&512)) refresh();
 }
 #ifdef X11
  XFlush(display);
 #endif
}

/***************************************************************************/

void gopipe(editor *p)
{
 int ok=0;
 int r,rf;
 char ch;
 rf=0;

 do
 {
  r=read(p->pipefd,&ch,1);

  if (r>0)
  {
   if (ch==9)
   {
    do
    {
     p->ppbuf[p->pppos++]=' '; p->ppbuf[p->pppos]='\0';
    } while((p->pppos)&7);
   }
   else
   if (ch==8) { if (p->pppos>0) p->pppos--; }
   else
   if (ch!=10 && ch!=13) { p->ppbuf[p->pppos++]=ch; p->ppbuf[p->pppos]='\0'; }
   if (ch==10 || p->pppos>=config.maxx)
   {
    p->add(p->ppbuf,p->pppos,3); p->pppos=0; rf=1;
    p->setmodif();
   }
  }
 } while(r>0);

 if (r!=0) ok=1; // la pipe esiste ancora
 else
 { // piu'
  close(p->pipefd); pipes--;
  if (p->pppos>0)
  {
   p->add(p->ppbuf,p->pppos,3); p->pppos=0; rf=1;
   p->setmodif();
  }
  p->add(config.strs+config.streop,strlen(config.strs+config.streop),3);
  p->setmodif();
  delete p->ppbuf;
  p->ppbuf=0; p->pipefd=0; p->pppos=0;
  rf=1;
 }

 if (rf)
 {
  int cx,cy;

  vsavecur(&cx,&cy);
  for (int j=0; j<winn; j++) if (win[j].edtr==p)
  {
   win[j].edtr->getup(win[j].myda,win[j].mya);
   win[j].edtr->puthdr(3);
  }
  vrestcur(cx,cy);
 }
 refresh();
}

#endif

/***************************************************************************/

void chkwinsize(void)
{
 int i;

// init(1);
 if (winn>0)
 {
  for (i=0; i<winn; i++)
  {
   if (win[i].myda+3>config.maxy)
   { if (i==0) win[i-1].mya=config.maxy-1; winn=i; }
   else
   if (win[i].mya>config.maxy-1)
   { win[i].mya=config.maxy-1; winn=i+1; }
  }
  if (cwin>=winn) cwin=0;
  if (win[winn-1].mya<config.maxy) win[winn-1].mya=config.maxy-1;
 }
}

/***************************************************************************/
// riscrive la finestra dell'editor dato solo se non e' la corrente

void winchk(editor *edtr)
{
 int ok=0;

 for (int i=0; i<winn; i++)
  if (win[i].edtr==edtr/* && i!=cwin*/)
   { win[i].edtr->getup(win[i].myda,win[i].mya); ok=1; }
 if (ok) win[cwin].edtr->getup(win[cwin].myda,win[cwin].mya);
}

/***************************************************************************/
// if type==1 non getuppa la corrente !

void wup(int type)
{
 for (int i=0; i<winn; i++)
 {
  if (!type || i!=cwin)
  {
   win[i].edtr->getup(win[i].myda,win[i].mya);
   win[i].edtr->puthdr(3);
  }
 }
}

/***************************************************************************/

editor *bufsel(int yda)
{
 int err;
 editor *r;

 r=0;

 if (edt.rtot()>0)
 {
  unzoomn=0;
  selez slz("Buffers");
  int i,ok;
  char str[140],*ps;
  editor *p;
  int ok2=0;

  edt.nseek(0); err=0;
  for (i=0; i<edt.rtot(); i++)
  {
   p=*(editor **)edt.rdati();

   ok=1;
   for (int j=0; j<winn; j++) if (win[j].edtr==p) { ok=0; break; }

   if (ok)
   {
    ps=p->rfilename();
    ok2=1;
    if (slz.add(ps,strlen(ps)+1,slz.sseek(ps)))
    {
     err=3;
     confirm(s_oom,0,win[cwin].myda,config.col[5]);
     break;
    } // no mem
   }
   edt.next();
  }

  if (ok2)
  {
   if (err==0) err=slz.goselez(str,yda,0); // 0:enter, 1:esc
   if (err==0)
   {
    edt.nseek(0);
    for (i=0; i<edt.rtot(); i++)
    {
     ps=(*(editor **)edt.rdati())->rfilename();
     if (strcmp(ps,str)==0) { r=*(editor **)edt.rdati(); break; }
     edt.next();
    }
   }
  }
 }
 return(r);
}

/***************************************************************************/

/*
*  1 : quit singolo di editor e finestra se finestra unica e editor non unico,
*      chiudo solo il file "e_quit"
* 17 : chiedi conferma e passa a 1 se ok
*  2 : split current window & ask filename "e_split"
*  3 : quit all editor & finestre - chiude tutto
* 18 : come 3 con conferma se file non salvati
* 19 : salva tutti gli editor
*  5 : delete current editor than 6 (substitute file in win)
*  6 : ask filename & load in current finestra
*  7 : resize win corrente
*  9 : goto next finestra con suo editor
* 10 : goto prev finestra con suo editor
* 11 : goto next editor
* 12 : goto prev editor
* 13 : elimino tutte le finestre fuorche' quella attuale (zoom/unzoom).
* 14 : elimino tutti gli editor/finestre fuorche' quello selezionato
* 20 : mostra lista di file in memoria per la selezione di uno
* 21 : come 1 ma ho gia' deletato l'editor - interno
* 22 : ask for new filename
* 23 : save clipboard
* 24 : load clipboard
* 27 : compile config file
* 28 : redraw all (ansi)
* 31 : param os-shell
* 32 : hide window
* 33 : reopen file
* 34 : come 33 senza conferma
* 35 : save window
* 36 : goto window
* 37 : salva tutti gli editor solo se modificati
* 38 : seleziona nome_file: la finestra o, se non c'e': ne crea associando
       o creando il buffer
*/

void mainswitch(int w)
{
 switch(w)
 {
  case 35 : // save window
  case 36 : // goto window
  {
   if (w==35) savecwin=cwin;
   else
   {
    if (savecwin>-1 && savecwin<winn) { unzoomn=0; cwin=savecwin; savecwin=-1; }
   }
  }
  break;
  case 34 : // reopen senza conferma
  case 33 : // reopen file
  {
   char file[128];

   strcpy(file,win[cwin].edtr->rfilename());
   unzoomn=0;
   if (w==33 && win[cwin].edtr->modif() && win[cwin].edtr!=clip.edtr
                               && win[cwin].edtr!=delbuf
                               && !win[cwin].edtr->getreadonly())
   {
    char r;
    r=confirm("File Modified, Save (Y/N/Esc) ?","yYnN",win[cwin].myda,config.col[6]);
    if (r=='y') { if (win[cwin].edtr->savefile()) break; }
    else if (r==(char)255) break;
   }

   win[cwin].edtr->loadfile(file,0);
  }
  break;
  case 32 : // hide window
   unzoomn=0;
   if (winn>1)
   {
    win[cwin].edtr->back();

    if (cwin>0)
    {
     win[cwin-1].mya=win[cwin].mya;
     memmove(&win[cwin],&win[cwin+1],((config.maxy/3+1)-cwin)*sizeof(_wind));
     cwin--;
    }
    else
    {
     memmove(&win[0],&win[1],((config.maxy/3+1)-cwin)*sizeof(_wind));
     if (win[0].myda!=-1) win[0].myda=0;
    }
    winn--;
   }
   winchk(nedt); win[cwin].edtr->getup(win[cwin].myda,win[cwin].mya);
  break;
  case 31 : // os shell con parametri
  {
   char prg[120],par[120],pat[80],*fn,*p,save[120];
   dialog ask;
   int err=0,pausa,d;
   int piped=0,type=0,statex=0,nowait=0;

   prg[0]='\0'; par[0]='\0'; pausa=0; fn=win[cwin].edtr->rfilename(); d=0;
   if (ask.ask("Program to execute (without parameter, enter=shell)",
       win[cwin].myda,prg)) break;
   #ifdef MSDOS
    if (prg[0]=='\0') strcpy(prg,getenv("COMSPEC"));
   #else
    if (prg[0]=='\0') strcpy(prg,getenv("SHELL"));
   #endif
   if (ask.ask("Parameter(s)",win[cwin].myda,par)) break;
   do
   {
    d=0;
    if ((p=strstr(par,"%d"))!=0) { *(p++)=*fn; d=1; }
    else
    if ((p=strstr(par,"%t"))!=0)
    {
     int l;

     strcpy(pat,fn+2);
     char *p2=strrchr(pat,'/'); if (p2) *(p2)='\0';
     l=strlen(pat);
     if (strlen(par)+l<120)
     { strcpy(save,p); strcpy(p+l,save); memcpy(p,pat,l); p+=l; }
     d=2;
    }
    else
    if ((p=strstr(par,"%f"))!=0)
    {
     int l;

     char *p2=strrchr(fn,'/');
     if (p2)
     {
      strcpy(pat,p2+1);
      p2=strrchr(pat,'.'); if (p2) *(p2)='\0';
      l=strlen(pat);
      if (strlen(par)+l<120)
      { strcpy(save,p); strcpy(p+l,save); memcpy(p,pat,l); p+=l; }
     }
     d=2;
    }
    else
    if ((p=strstr(par,"%n"))!=0)
    {
     int l;

     char *p2=strrchr(fn,'/');
     if (p2)
     {
      strcpy(pat,p2+1);
      l=strlen(pat);
      if (strlen(par)+l<120)
      { strcpy(save,p); strcpy(p+l,save); memcpy(p,pat,l); p+=l; }
     }
     d=2;
    }
    else
    if ((p=strstr(par,"%e"))!=0)
    {
     int l;

     char *p2=strrchr(fn,'.');
     if (p2)
     {
      strcpy(pat,p2+1);
      l=strlen(pat);
      if (strlen(par)+l<120)
      { strcpy(save,p); strcpy(p+l,save); memcpy(p,pat,l); p+=l; }
     }
     d=2;
    }
    else if ((p=strstr(par,"%x"))!=0) { type=1; d=2; }
    else if ((p=strstr(par,"%p"))!=0) { pausa|=1; d=2; }
    else if ((p=strstr(par,"%r"))!=0) { pausa|=2; d=2; }
    else if ((p=strstr(par,"%i"))!=0) { piped=1; d=2; }
    else if ((p=strstr(par,"%a"))!=0) { piped=2; d=2; }
    else if ((p=strstr(par,"%k"))!=0) { statex=1; d=2; }
    else if ((p=strstr(par,"%s"))!=0) { statex=2; d=2; }
    else if ((p=strstr(par,"%o"))!=0) { nowait=1; d=2; }
    else if ((p=strstr(par,"%w"))!=0)
    {
     win[cwin].edtr->word(pat);
     int l=strlen(pat);
     if (strlen(par)+l<120)
     { strcpy(save,p); strcpy(p+l,save); memcpy(p,pat,l); p+=l; }
     d=2;
    }
    else if ((p=strstr(par,"%v"))!=0 && (savecwin>=0 && savecwin<winn))
    {
     win[savecwin].edtr->word(pat);
     int l=strlen(pat);
     if (strlen(par)+l<120)
     { strcpy(save,p); strcpy(p+l,save); memcpy(p,pat,l); p+=l; }
     d=2;
    }
    if (d) strcpy(p,p+d);

   } while (d>0);
   if (statex) mainswitch(37); // salvo i file modificati
   if (piped && statex==1) statex=0;

   if (!piped) clearscr();
   if (type) output(" Type \"exit\" to return to Zed\n\r\n");
   #ifdef MSDOS
    char *vect[20],*pn;
    int ii=0;

    pn=par; vect[ii++]=prg;

    while (ii<18)
    {
     while(*pn==' ') pn++;
     vect[ii++]=pn;
     while(*pn!=' ' && *pn!=0) pn++;
     if (*pn==0) break; else *(pn++)='\0';
    }
    vect[ii]=0;

    err=spawnv(P_WAIT,prg,vect);
   #else
   {
    int pp[2],pid=0;

    if (piped) pipe(pp); else deinit(); // ripristino i settaggi del terminale

    if ((pid=fork())==0)
    { // figlio
     char *argv[100];
     int argc=0;

     char *pn,*sp;
     int len;
     pn=par;

     argv[argc++]=prg;

     while (argc<99)
     {
      while(*pn==' ') pn++;
      sp=pn; len=0;
      if (*pn==34)
      {
       pn++; sp=pn; while(*pn!=34 && *pn!=0) { pn++; len++; } pn++;
      } else while(*pn!=' ' && *pn!=0) { pn++; len++; }
      if (len>0)
      {
       argv[argc]=new char[len+1];
       memcpy(argv[argc],sp,len); argv[argc][len]='\0'; argc++;
      }
      if (*pn==0) break;
     }
     argv[argc]=0;

     if (piped)
     {
      close(pp[0]);
      close(0); close(1); close(2);
      dup2(pp[1],1); dup2(pp[1],2);
     }
     execvp(argv[0],argv);
     if (piped)
     {
      write(2,"Executable ",11);
      write(2,argv[0],strlen(argv[0]));
      write(2," not found !\n",13);
      close(1); close(2);
     }
     exit(1); // !
    }

    if (piped)
    {
     close(pp[1]);
     fcntl(pp[0],F_SETFL,O_NONBLOCK);
     if (piped==1) win[cwin].edtr->reset();
     win[cwin].edtr->add(config.strs+config.strsop,strlen(config.strs+config.strsop),3);
     win[cwin].edtr->setmodif();
     win[cwin].edtr->setpipe(pp[0]);

     pipes++;
    }
    else
    {
     int status;

     if (!nowait)
     {
      waitpid(pid,&status,0);
      if (!status) err=1; else err=WEXITSTATUS(status);
     } else err=0;
    }
   }
   #endif

   if (!piped)
   {
    #ifndef X11
     if ((pausa&1) || ((pausa&2) && err>0))
     { output("\nAny key to return to ZED..."); gettst2(); }
    #endif

    preinit();
    init(1);
    chkwinsize(); // check video
    if (statex)
    {
     int i,t;

     t=edt.rtot(); // numero di editor
     edt.nseek(0);
     for (i=0; i<t; i++)
     {
      editor *edtr;
      struct stat newst;

      edtr=*((editor **)edt.rdati());

      if (stat(edtr->rfilename(),&newst)!=-1)
      {
       newst.st_nlink=0;
       newst.st_atime=0;

       if (memcmp(&newst,&edtr->stato,sizeof(struct stat))!=0)
       {
        edtr->loadfile(edtr->rfilename(),0);
       }
      }

      edt.next();
     }
    }
    wup();
   }
  }
  break;
  case 28 : // redraw all
  if (config.ansi)
  {
   init(1);
   clearscr();
   chkwinsize(); // check video
   wup();
  } break;
  case 27 : // recompile config file
  {
   char *saveconfigs;
   _config savecfg;
   unsigned int *savekey;
   unsigned char *savecmd;
   char *savestr;
   _menu *savemnu;
   unsigned int savepkeys,savepcmds;
   unsigned int savepstrs,savepmnus;
   #ifdef X11
    int savecol[MAXCOLORS];

    memcpy(savecol,config.col,sizeof(config.col));
   #endif

   #ifndef X11
    clearscr();
    deinit();
   #endif

   savepkeys=config.pkeys;  // salvo la vecchia configurazione
   savepcmds=config.pcmds;
   savepstrs=config.pstrs;
   savepmnus=config.pmnus;

   savekey=new unsigned int [config.pkeys];
   savecmd=new unsigned char [config.pcmds];
   savestr=new char [config.pstrs];
   savemnu=new _menu [config.pmnus];
   memcpy(savekey,config.keys,config.pkeys*4);
   memcpy(savecmd,config.cmds,config.pcmds);
   memcpy(savestr,config.strs,config.pstrs);
   memcpy(savemnu,config.mnus,config.pmnus*sizeof(_menu));
   savecfg=config;

   delete config.keys; delete config.cmds;
   delete config.strs; delete config.mnus;
   config.keys=0; config.cmds=0;
   config.strs=0; config.mnus=0;

   saveconfigs=strdup(configs);

   memset(&config,0xff,sizeof(_config));     // azzero la config

   if (lconfig(0,0)!=0)
   {
    resetcfg(0,"old");
    #ifndef X11
     output("\n Press ENTER to continue...\n");
     gettst3();
    #endif
    delete configs; configs=saveconfigs;

    config=savecfg;
    config.pkeys=savepkeys; // restore old configuration
    config.pcmds=savepcmds;
    config.pstrs=savepstrs;
    config.pmnus=savepmnus;

    config.keys=new unsigned int [savepkeys];
    config.cmds=new unsigned char [savepcmds];
    config.strs=new char [savepstrs];
    config.mnus=new _menu [savepmnus];

    memcpy(config.keys,savekey,savepkeys*4);
    memcpy(config.cmds,savecmd,savepcmds);
    memcpy(config.strs,savestr,savepstrs);
    memcpy(config.mnus,savemnu,savepmnus*sizeof(_menu));
   } else { defaultok(); delete saveconfigs; }

   delete savekey; delete savecmd; delete savestr; delete savemnu;
   exectst((int *)4); // azzero i puntatori eventuali

   #ifndef X11
    preinit();
   #endif
   init(1);
   #ifndef X11
    clearscr();
   #endif

   #ifdef X11
    memcpy(config.col,savecol,sizeof(config.col));
//    for (int i=0; i<MAXCOLORS; i++) config.col[i]=i;
   #endif

   chkwinsize(); // check video
   wup();
  } break;
  case 22 : // ask for new filename
   unzoomn=0;
   askfile(0,0,win[cwin].myda,win[cwin].mya,win[cwin].edtr);
  break;
  case 23: // save clipboard
   if (askfile(0,0,win[cwin].myda,win[cwin].mya,clip.edtr)!=(editor *)2)
   {
    clip.edtr->ydaset(win[cwin].myda);
    clip.edtr->savefile();
   }
  break;
  case 24: // load clipboard
   askfile(0,0,win[cwin].myda,-1,0);
   clip.cstart=0; clip.cend=1023;
   clip.blktype=0;
  break;
  case 20 : // mostra lista di file in memoria per la selezione di uno
  {
   editor *ret=bufsel(win[cwin].myda);
   if (ret!=0) win[cwin].edtr=ret;
  }
  break;
  case 14 : // elimino tutti gli editor/finestre fuorche' quello selezionato
  {
   editor *cons; // questo e' l'editor da tenere
   int ok;

   if (win[cwin].edtr==clip.edtr || win[cwin].edtr==delbuf) break;

   mainswitch(13); wup(); // zoommo la finestra
   unzoomn=0;

   edt.nseek(0); // vado al primo
   cons=win[0].edtr;
   ok=1;

   while (edt.rtot()>1 && ok)
   {
    int tot;
    if ((*(editor **)edt.rdati())==cons) edt.next();
    win[0].edtr=(*(editor **)edt.rdati());
    tot=edt.rtot();
    mainswitch(17); // query & quit
    if (tot==edt.rtot()) ok=0;
   }
  }
  break;
  case 13 : // elimino tutte le finestre fuorche' quella attuale (zoom).
   if (unzoom)
   {
    memcpy(win,unzoom,sizeof(_wind)*unzoomn); cwin=unzoomc; winn=unzoomn;
    wup();
    unzoomn=0;
   }
   else
   {
    if ((unzoom=new _wind[config.maxy/3+1])!=0)
    {
     int i;

     for (i=0; i<winn; i++) if (i!=cwin) win[i].edtr->back();

     memcpy(unzoom,win,sizeof(_wind)*winn); unzoomc=cwin; unzoomn=winn;
     win[0]=win[cwin];
     cwin=0; winn=1; win[0].myda=0; win[0].mya=config.maxy-1;
    }
   }
  break;
  case 5 : // delete current editor, ask (substitute file in win)
  {
   int oldcwin=cwin;

   unzoomn=0;
   if (win[cwin].edtr->modif() && win[cwin].edtr!=clip.edtr
                               && win[cwin].edtr!=delbuf
                               && !win[cwin].edtr->getreadonly())
   {
    char r;
    r=confirm("File Modified, Save (Y/N/Esc) ?","yYnN",win[cwin].myda,config.col[6]);
    if (r=='y') { if (win[cwin].edtr->savefile()) break; }
    else if (r==(char)255) break;
   }

   nedt=win[cwin].edtr;
   if (edt.aseek((char *)&nedt,sizeof(editor *)))
   {
    delete *(editor **)edt.rdati();
    edt.remove();
   }

   // come 6 MA ho in piu' l'else!!!!!
   nedt=askfile(0,0,win[cwin].myda,win[cwin].mya,0);
   if (nedt==(editor *)1) confirm(s_oom,0,win[cwin].myda,config.col[5]);
   if (nedt>(editor *)3) win[cwin].edtr=nedt;
   else
   {
    int redr=0;
    unzoomn=0;
    if (winn==1 && edt.rtot()>0) win[cwin].edtr=*((editor **)edt.rdati());
    else
    {
     if (oldcwin>0)
     {
      int i=oldcwin-1;

      win[i].mya=win[oldcwin].mya;
      win[i].edtr->getup(win[i].myda,win[i].mya);
      win[i].edtr->puthdr(3);
      memmove(&win[oldcwin],&win[oldcwin+1],
              ((config.maxy/3+1)-oldcwin)*sizeof(_wind));
      if (cwin>=oldcwin) cwin--;
      redr=1;
     }
     else
     {
      memmove(&win[0],&win[1],((config.maxy/3)-oldcwin)*sizeof(_wind));
      if (win[0].myda!=-1) win[0].myda=0;
      win[0].edtr->getup(win[0].myda,win[0].mya);
      win[0].edtr->puthdr(3);
      if (cwin>0) cwin--;
      redr=1;
     }

     if (redr && nedt==(editor *)3)
     {
      win[cwin].edtr->andflag(~FL_SCRITT);
      win[cwin].edtr->settodraw(0);

      win[cwin].edtr->puthdr(3);
      vputstr("Sel Ok",6,win[cwin].myda,config.col[6]);
      win[cwin].edtr->orflag(FL_SCRITT);
     }

     winn--;
    }

   }
  } break;
  case 6 : // ask filename & load in current finestra
  {
   unzoomn=0;
   nedt=askfile(0,0,win[cwin].myda,win[cwin].mya,0);
   if (nedt==(editor *)1) confirm(s_oom,0,win[cwin].myda,config.col[5]);
   if (nedt>(editor *)3) win[cwin].edtr=nedt;
  }
  break;
  case 11 : // goto next editor
  {
   int ok,i;
   editor *p=0;

   unzoomn=0;
   nedt=win[cwin].edtr;
   if (edt.aseek((char *)&nedt,sizeof(editor *)))
   do
   {
    ok=0;
    edt.next();
    p=*(editor **)edt.rdati();

    for (i=0; i<winn; i++)
     if (win[i].edtr==p && i!=cwin) { ok=1; break; }
   } while (ok);
   win[cwin].edtr=p;
  }
  break;
  case 12 : // goto prev editor
  {
   int ok,i;
   editor *p=0;

   unzoomn=0;
   nedt=win[cwin].edtr;
   if (edt.aseek((char *)&nedt,sizeof(editor *)))
   do
   {
    ok=0;
    edt.prev();
    p=*(editor **)edt.rdati();

    for (i=0; i<winn; i++)
     if (win[i].edtr==p && i!=cwin) { ok=1; break; }
   } while (ok);
   win[cwin].edtr=p;
  }
  break;
  case 9 : // goto next finestra con suo editor
   unzoomn=0; cwin++; if (cwin>=winn) cwin=0;
  break;
  case 10 : // goto prev finestra con suo editor
   unzoomn=0; if (cwin==0) cwin=winn-1; else cwin--;
  break;
  case 7 : // resize win corrente
  {
   if (cwin==0 && winn>1) // se e' la prima, resizo la seguente
   {
    mainswitch(9); // goto next finestra con suo editor
    mainswitch(7); // resize
    win[cwin].edtr->getup(win[cwin].myda,win[cwin].mya);
    mainswitch(10); // goto prev finestra con suo editor
   }

   if (cwin>0)
   {
    unzoomn=0;

    int ret,d;

    do
    {
     vputstr("Use arrow to resize window...",29,win[cwin].myda,config.col[6]);
     ret=0;
     d=0;
     if (exectst(&d)==2)
     switch(d)
     {
      case 2 : // freccia giu'
      if (cwin>0 && (win[cwin-1].mya-win[cwin-1].myda)>2)
      {
       win[cwin-1].mya--; win[cwin].myda--;
       win[cwin-1].edtr->getup(win[cwin-1].myda,win[cwin-1].mya);
       win[cwin].edtr->getup(win[cwin].myda,win[cwin].mya);
      }
      break;
      case 1 : // frecciasu
      if (cwin>0 && (win[cwin].mya-win[cwin].myda)>2)
      {
       win[cwin-1].mya++; win[cwin].myda++;
       win[cwin-1].edtr->getup(win[cwin-1].myda,win[cwin-1].mya);
       win[cwin].edtr->getup(win[cwin].myda,win[cwin].mya);
      }
      break;
      case 12:
      case 136: ret=1;
     }
    } while (!ret);
    win[cwin].edtr->puthdr(3);
   }
  }
  break;
  case 3 : // quit all - to check - se se qualcuno ha escato ?
   unzoomn=0; while (winn>0) mainswitch(1);
  break;
  case 18: // come 3 con conferma se file non salvati
  {
   int t;
   unzoomn=0;
   mainswitch(13); wup(); // zoommo la finestra

   if (win[cwin].edtr==clip.edtr || win[cwin].edtr==delbuf) mainswitch(17);

   do
   {
    t=edt.rtot(); mainswitch(17);
   } while(t!=edt.rtot() && edt.rtot()>0);

  } break;
  case 19 : // salva tutti gli editor
  {
   int i,t;

   t=edt.rtot(); // numero di editor
   edt.nseek(0);
   for (i=0; i<t; i++)
   { (*((editor **)edt.rdati()))->savefile(); edt.next(); }
   for (i=0; i<winn; i++) win[i].edtr->puthdr(4); // risistemo gli header
  } break;
  case 37 : // salva tutti gli editor solo se modificati
  {
   int i,t;

   t=edt.rtot(); // numero di editor
   edt.nseek(0);
   for (i=0; i<t; i++)
   {
    if ((*((editor **)edt.rdati()))->modif())
     (*((editor **)edt.rdati()))->savefile();
    edt.next();
   }
   for (i=0; i<winn; i++) win[i].edtr->puthdr(4); // risistemo gli header
  } break;
  case 17 : // chiedi conferma e passa a 1 se ok
   unzoomn=0;
   if (win[cwin].edtr->modif() && win[cwin].edtr!=clip.edtr
                               && win[cwin].edtr!=delbuf
                               && !win[cwin].edtr->getreadonly())
   {
    char r;
    r=confirm("File Modified, Save (Y/N/Esc) ?","yYnN",win[cwin].myda,config.col[6]);
    if (r=='y') { if (win[cwin].edtr->savefile()) break; }
    else if (r==(char)255) break;
   } // fall in 1
  case 1 : // quit singolo di editor e finestra "e_quit"
   unzoomn=0;
   nedt=win[cwin].edtr;
   if (edt.aseek((char *)&nedt,sizeof(editor *))
       && win[cwin].edtr!=clip.edtr && win[cwin].edtr!=delbuf)
   {
    delete *(editor **)edt.rdati();
    edt.remove();
   } // fall in 21
  case 21 : // come 1 ma ho gia' deletato l'editor - interno
   unzoomn=0;
   if (winn==1 && edt.rtot()>0) win[cwin].edtr=*((editor **)edt.rdati());
   else
   {
    if (cwin>0)
    {
     win[cwin-1].mya=win[cwin].mya;
     memmove(&win[cwin],&win[cwin+1],((config.maxy/3+1)-cwin)*sizeof(_wind));
     cwin--;
    }
    else
    {
     memmove(&win[0],&win[1],((config.maxy/3)-cwin)*sizeof(_wind));
     if (win[0].myda!=-1) win[0].myda=0;
    }
    winn--;
   }
   if (winn>0)
   {
    winchk(nedt); win[cwin].edtr->getup(win[cwin].myda,win[cwin].mya);
    win[cwin].edtr->puthdr(3);
   }
  break;
  case 2 : // split current window & ask filename "e_split"
   unzoomn=0;
   if (win[cwin].mya-win[cwin].myda>5) // se c'e' spazio
   {
    int yda1,ya1,err,ret;

    yda1=win[cwin].myda+(win[cwin].mya-win[cwin].myda)/2;
    ya1=win[cwin].mya;

    err=0; ret=0;

    nedt=askfile(0,0,yda1+1,ya1,0);

    if (nedt==(editor *)1) confirm(s_oom,0,win[cwin].myda,config.col[5]);
    if (nedt>(editor *)3)
    {
     memmove(&win[cwin+1],&win[cwin],((config.maxy/3)-cwin)*sizeof(_wind));
     win[cwin+1].mya=ya1;
     win[cwin].mya=yda1;
     win[cwin+1].myda=yda1+1;
     win[cwin+1].edtr=nedt;
     cwin++; winn++;
    }
    wup(1);
   }
  break;
  case 38 :

//chiede nome_file: se c'e' la sua finestra la seleziona, else
//se c'e' il suo buffer splitta la finestra e lo seleziona, se non
//riesce a splittare la finestra sostituisce la finestra corrente
//se non c'e' il buffer crea un nuovo buffer con quel nome

//in pratica agisce esattamente come "case 2" ma se non riesce a splittare
//la finestra allora fa un load file in finestra corrente "case 6"

   if (win[cwin].mya-win[cwin].myda>5) mainswitch(2); // se c'e' spazio
   else mainswitch(6);
  break;
 }
 if (unzoomn==0 && unzoom) { delete unzoom; unzoom=0; }
}

/***************************************************************************/

int main(int argc,char *argv[])
{
 int err=0,ret,myargc=1;

 #ifdef MSDOS
//  _go32_want_ctrl_break(1);
//  setcbrk(0);
  signal(SIGINT,SIG_IGN);
  argv0=argv[0];
 #endif

 memset(&config,0xff,sizeof(_config));     // azzero la config
 config.keys=0; config.cmds=0; config.strs=0; config.mnus=0;
 config.pkeys=config.pcmds=config.pstrs=config.pmnus=0;

 {
  char nome[512],*p;
  int pause=0;

  nome[0]='\0';
  if ((p=getenv("ZEDCONFIG"))!=0) strcpy(nome,p);

  myargc=parsecm(argc,argv,nome); // parse command line param

  outtestata();

  if (myargc==-1)
  {
   #ifdef X11
    output("\
  usage: zedx [option]... [path/file] [path/file]...\n\
    ");
   #else
    output("\
  usage: zed [option]... [path/file] [path/file]...\n\
    ");
   #endif
    output("\
   [option]:\n\
    +####     : goto line #\n\
    -cfg name : specify config file\n\
    -add name : add file to configuration\n\
    -def      : use directly default config file\n\
    --        : force end of parameters\n\
    -nospace on/off    -coltoscroll xx   -columns xx        -rows xx\n\
    -restoredir on/off -maxdelbuf xx     -showmatch on/off  -ansi on/off/xx\n\
    -indent on/off     -showtab on/off   -tabexpandl on/off -tabexpandr on/off\n\
    -rmargin xx        -lmargin xx       -jindent xx        -doc on/off\n\
    -softret xx        -wholeline on/off -bak on/off        -usrvar xx\n\
    -colormode xx (0=disable,1=auto,2=c++,3=html,4=tex,5=mail)\n");

 #ifdef UNIX
 output("\
    -delay xx          -shiftlinux on/off\n");
 #endif
 #ifdef X11
 output("\
    -display host      -font font        -geometry geometry\n");
 #endif
 output("\
    -col(status/text/curr/eof/dialog/error/warning/sel/currsel/select/block)\n\
    -col(fndtext/currblk/match/para/bpara)\n\
 \n");
   exit(1);
  }

  err=0;

  if (nome[0])
  {
   err=lconfig(0,nome);
   if (err) { resetcfg(myargc,"local"); pause=1; }
  }

  if ((err || nome[0]=='\0') && config.config==1)
  {
   err=lconfig(1,0);
  }
  else
  if ((err || nome[0]=='\0') && config.config!=1)
  {
   if (lconfig(3,0)==0)
   {
    err=lconfig(0,0);
    if (err)
    {
     resetcfg(myargc,"default"); pause=1;
     err=lconfig(1,0);
     //  if (err) resetcfg(myargc,"internal");
     //  err=lconfig(2,0);
    }
   } else
   {
    output("Local config not found...\n");
    err=lconfig(1,0);
   }
  }

  if (err || config.pkeys==0 || config.pcmds==0)
  {
   output("\nNo key/command on loaded configuration... exiting.\n");
   exit(1);
  }
  else defaultok();

  if (pause)
  {
   output("\n Press ENTER to continue...\n");
   gettst3();
  }
 }

 if (err==0)
 {
  preinit();

  #ifdef X11
   err=init_display();
   if (err) { deinit(); exit(1); }
  #endif
  if (init(0)) { deinit(); exit(1); }

  #ifdef X11
   while (!zedmapped) getxkey();
  #endif

  #if defined UNIX && !defined X11
   signal(SIGWINCH,signals);
  #endif

  #ifdef UNIX
   signal(SIGCHLD,SIG_IGN);
  #endif

  #if defined MSDOS
   _go32_dpmi_registers regs;
   regs.x.ax=0x1003;
   regs.h.bl=0;
   regs.x.ss=regs.x.sp=0;
   _go32_dpmi_simulate_int(0x10,&regs);
  #endif

  if (config.doc==1)
  { config.nospace=1; config.tabexpl=1; config.shwtab=1; }
  if (config.tabexpr) config.shwtab=1;
 }

 err=0; ret=0; nedt=0; win=0; spath=0; clip.edtr=0; delbuf=0; winn=-1;

 if (err==0) if ((spath=new char[128])==0) err=1;
 else
 {
  spath[0]='\0';
  _getcwd(spath,128);

  if (err==0) if ((win=new _wind[config.maxy/3+1])==0) err=1;
  if (err==0) if ((clip.edtr=new editor(0,config.maxy-1))==0) err=1;
  if (err==0) if ((delbuf=new editor(0,config.maxy-1))==0) err=1;

  if (err==0)
  {
   do
   {
    err=0;
    dialof=65536;
    printlogo();

    if ((nedt=askfile(argc-myargc+1,&argv[myargc-1],0,config.maxy-1,0))<(editor *)4) err=1;
   } while ((dialof&65536)==0 && err==1);
   dialof=0;
  }
 }

 if (err)
 {
  if (nedt!=clip.edtr && nedt!=delbuf && nedt>(editor *)3) { nedt=0; err=11; }
 }
 if (nedt==(editor *)2) err=10; else win[0].edtr=nedt;

 if (err<10 && err) confirm("Out Of Memory, hit ESC to exit",0,0,config.col[5]);

 if (err==0)
 {
  win[0].myda=0; win[0].mya=config.maxy-1;
  win[1].myda=-1; winn=1; cwin=0;

  do // ciclo centrale dell'editor
  {
   win[cwin].edtr->getup(win[cwin].myda,win[cwin].mya);

   ret=win[cwin].edtr->looptst();
   mainswitch(ret);
  } while(edt.rtot()!=0); // ciclo finche' ci sono editor aperti
 }

 _chdir(spath); delete spath;

 delete win; delete clip.edtr; delete delbuf;
 delete config.keys; delete config.cmds;
 delete config.strs; delete config.mnus;
 delete s_str; delete s_strto;

 err=0;

 #ifndef X11
  if (config.ansi>0 && config.ansi<128) outansi("\x1b[0m");
  clearscr();
 #endif

 #if defined MSDOS
  _go32_dpmi_registers regs;
  regs.x.ax=0x1003;
  regs.h.bl=1;
  regs.x.ss=regs.x.sp=0;
  _go32_dpmi_simulate_int(0x10,&regs);
 #endif

 deinit();

 return(0);
}

/***************************************************************************/
