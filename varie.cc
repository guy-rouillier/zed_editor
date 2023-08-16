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

#define MAXX 80
#define MAXLN (MAXX-2)

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <regex.h>
#ifdef X11
 #include <X11/Xlib.h>
 #include <X11/keysym.h>
#endif

#ifdef _AIX
 #include <strings.h>
#endif

#include "zed.h"

#ifdef MSDOS
 #include <dir.h>
 #include <go32.h>
 #include <io.h>
#endif

/***************************************************************************/
/* XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX */
/***************************************************************************/
/* lista.cc - gestione di una generica lista doppio linkata */
/***************************************************************************/

/***************************************************************************/

//inline int min(int v1,int v2)
//{ return ((v1<v2)?v1:v2); }

/***************************************************************************/
// sostituisce l'elemento corrente con quello dato, ret=0=Ok

int list::subst(char *data,int len)
{
 elem *nnn,*nn2;
 int l=len&0x03ff;

 if ((nnn=(list::elem *)new(char [sizeof(elem)+l]))==0) return(1);

 memcpy(nnn->dati,data,l);
 nnn->lung=len;
 nnn->prec=corr->prec; nnn->pros=corr->pros;
 if (nnn->prec) nnn->prec->pros=nnn;
 if (nnn->pros) nnn->pros->prec=nnn;
 if (prima==corr) prima=nnn;
 if (ulti==corr) ulti=nnn;
 nn2=corr; corr=nnn; delete nn2;

 return(0);
}

/***************************************************************************/
// sostituisce l'elemento dato con quello dato, ret=0=Ok

int list::isubst(elem **sc,char *data,int len)
{
 elem *nnn,*nn2;
 int l=len&0x03ff;

 if ((nnn=(list::elem *)new(char [sizeof(elem)+l]))==0) return(1);

 memcpy(nnn->dati,data,l);
 nnn->lung=len;
 nnn->prec=(*sc)->prec; nnn->pros=(*sc)->pros;
 if (nnn->prec) nnn->prec->pros=nnn;
 if (nnn->pros) nnn->pros->prec=nnn;
 if (prima==(*sc)) prima=nnn;
 if (ulti==(*sc)) ulti=nnn;
 if (corr==(*sc)) corr=nnn;
 nn2=(*sc); (*sc)=nnn; delete nn2;

 return(0);
}

/***************************************************************************/
/* aggiunge un elemento alla lista in posizione "modo"
  switch modo:
   case 1 : ins elemento prima del corrente
   case 2 : ins elemento dopo il corrente
   case 3 : ins elemento dopo l'ultimo (alla fine)
  return:
   0 : all ok
   1 : non c'e' abbastanza memoria */

int list::add(char *data,int len,int modo)
{
 elem *nnn;
 int l=len&0x03ff;

 // creo il nuovo elemento

 if ((nnn=(list::elem *)new(char [sizeof(elem)+l]))==0) return(1);
 memcpy(nnn->dati,data,l);
 nnn->lung=len;

 if (prima)
 { // esiste gia' un elemento
  switch(modo)
  {
   case 1 : // inserimento elemento prima del corrente
    if (corr->prec)
    { // se non e' il primo
     nnn->prec=corr->prec; nnn->pros=corr;
     corr->prec->pros=nnn; corr->prec=nnn;
    }
    else
    { // e' il primo
     nnn->prec=0; nnn->pros=corr;
     corr->prec=nnn; prima=nnn;
    }
    lcorr++; lulti++; // il corrente e l'ultimo incrementa di numero
   break;
   case 2 : // inserimento elemento dopo il corrente
    if (corr->pros)
    { // se non e' l'ultimo
     nnn->prec=corr;  nnn->pros=corr->pros;
     corr->pros->prec=nnn; corr->pros=nnn;
    }
    else
    { // e' l'ultimo
     nnn->prec=corr; nnn->pros=0;
     corr->pros=nnn; ulti=nnn;
    }
    lulti++; // l'ultimo si incrementa di numero
   break;
   case 3 : // inserimento elemento dopo l'ultimo
    nnn->prec=ulti; nnn->pros=0;
    ulti->pros=nnn; ulti=nnn;
    lulti++; // l'ultimo si incrementa di numero
   break;
  }
 }
 else
 { // e' il primo elemento della lista
  prima=ulti=corr=nnn; lcorr=lulti=0;
  nnn->prec=nnn->pros=0;
 }

 return(0);
}

/***************************************************************************/
// cancella dalla lista l'elemento corrente

void list::remove(void )
{
 elem *s=corr;

 if (s) // controllo che il corrente esista
 {
  if (s->prec!=0)
  {
   if (s->pros!=0)
   { // elemento generico
    s->pros->prec=s->prec; s->prec->pros=s->pros; corr=s->pros;
   }
   else
   { // e' l'ultimo ma non il primo
    s->prec->pros=0; corr=s->prec; lcorr--; ulti=corr;
   }
  }
  else
  {
   if (s->pros!=0)
   { // se e' il primo ma non l'ultimo
    s->pros->prec=0; corr=s->pros; prima=corr;
   }
   else
   { // e' l'unico elemento
    prima=corr=ulti=0; lcorr=0xffffffffL; lulti=0; // con il dec = 0xfff...
   }
  }
  delete s; // libero l'elemento
  lulti--;
 }
}

/***************************************************************************/
// distruttore della lista, elimino tutti gli elementi

void list::eraseall(void)
{
 while(corr) remove();
}

/***************************************************************************/
// costruttore della lista

void list::startup(void)
{
 prima=corr=ulti=0; lcorr=lulti=0xffffffffL;
}

/***************************************************************************/
// posiziona l'elemento corrente per numero
// ret=1 == OK

int list::nseek(int edn)
{
 int r;
 r=0;

 if (edn<=lulti)
 {
  if (lulti-edn>edn)
   for (lcorr=0,corr=prima; lcorr<edn; lcorr++,corr=corr->pros);
  else
   for (lcorr=lulti,corr=ulti; lcorr>edn; lcorr--,corr=corr->prec);
  r=1;
 }
 return(r);
}

/***************************************************************************/
// posiziona l'elemento corrente per numero
// ret=1 == OK

int list::inseek(elem **scorr,int &slcorr,int edn)
{
 int r;
 r=0;

 if (edn<=lulti)
 {
  if (lulti-edn>edn)
   for (slcorr=0,*scorr=prima; slcorr<edn; slcorr++,*scorr=(*scorr)->pros);
  else
   for (slcorr=lulti,*scorr=ulti; slcorr>edn; slcorr--,*scorr=(*scorr)->prec);
  r=1;
 }
 return(r);
}

/***************************************************************************/
// posiziona l'elemento corrente per contenuto elemento
// ret=1 == OK, trovato

int list::aseek(char *fd,int fl)
{
 int r;
 int n;
 elem *scorr;
 r=0;

 for (scorr=prima,n=0; scorr!=0; scorr=scorr->pros,n++)
  if ((scorr->lung&0x03ff)==fl) if (!memcmp(scorr->dati,fd,(size_t)fl))
  { corr=scorr; lcorr=n; r=1; break; }

 return(r);
}

/***************************************************************************/
// posiziona l'elemento corrente in ordine alfabetico per filenanme
// rispetto alla stringa in input e il contenuto degli elementi
// ret=modalita' di inserimento == OK

int list::sseek(char *str)
{
 int l,r;
 l=strlen(str);
 corr=prima; lcorr=0;

 r=-1;
 while (corr)
 {
  if (*corr->dati=='/' && *str!='/') r=-1;
  else
  if (*corr->dati!='/' && *str=='/') r=1;
  else r=memcmp(corr->dati,str,(corr->lung&0x03ff)<?l);
  if (r==0) if ((corr->lung&0x03ff)>l) r=1; else r=-1;

  if (r>-1) break;
  corr=corr->pros; lcorr++;
 }
 if (corr && corr->prec) { corr=corr->prec; lcorr--; r=2; } else r=1;
 if (!corr) { corr=ulti; lcorr=lulti; r=2; }
 return(r);
}

/***************************************************************************/

/***************************************************************************/
/* XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX */
/***************************************************************************/

/***************************************************************************/
/* dialog.cc - gestione dialog box */
/***************************************************************************/
// chiede qualcosa all'utente

int dialog::ask(char *sstr,int yda,char *instr,int special)
{
 int ret,cx,cy;
 svsave *save;

 tabexit=0; filespc=0;
 if (special&1) tabexit=1;
 if (special&2) filespc=1;

 config.gotoline=-1;
 if (dialof&2048) { dialof&=65536; return(1); }// caso abort da resize da selez

 dialof=256|(dialof&65536);
// mutex++;

 vsavecur(&cx,&cy);
 save=vsave(yda,yda+2);

 #ifdef MSDOS
 putchr('\311',0,yda,config.col[4]);
 putchr('\273',MAXX-1,yda,config.col[4]);

 putchr('\310',0,yda+2,config.col[4]);
 putchr('\274',MAXX-1,yda+2,config.col[4]);

 putchr('\272',0,yda+1,config.col[4]);
 putchr('\272',MAXX-1,yda+1,config.col[4]);

 putchn('\315',MAXX-2,1,yda  ,config.col[4]);
 putchn('\315',MAXX-2,1,yda+2,config.col[4]);
 #else
  #ifndef X11
  if (config.ansi!=3)
  {
   putchr('/',0,yda,config.col[4]);
   putchr('\\',MAXX-1,yda,config.col[4]);

   putchr('\\',0,yda+2,config.col[4]);
   putchr('/',MAXX-1,yda+2,config.col[4]);

   putchr('|',0,yda+1,config.col[4]);
   putchr('|',MAXX-1,yda+1,config.col[4]);

   putchn('-',MAXX-2,1,yda  ,config.col[4]);
   putchn('-',MAXX-2,1,yda+2,config.col[4]);
  }
  else
  {
   putchr('\311',0,yda,0);
   putchr('\273',MAXX-1,yda,0);

   putchr('\310',0,yda+2,0);
   putchr('\274',MAXX-1,yda+2,0);

   putchr('\272',0,yda+1,0);
   putchr('\272',MAXX-1,yda+1,0);

   putchn('\315',MAXX-2,1,yda  ,0);
   putchn('\315',MAXX-2,1,yda+2,0);
  }
  #else
  putchr(1,0,yda,config.col[4]|64);
  putchr(2,MAXX-1,yda,config.col[4]|64);

  putchr(3,0,yda+2,config.col[4]|64);
  putchr(4,MAXX-1,yda+2,config.col[4]|64);

  putchr(5,0,yda+1,config.col[4]|64);
  putchr(6,MAXX-1,yda+1,config.col[4]|64);

  putchn(7,MAXX-2,1,yda  ,config.col[4]|64);
  putchn(8,MAXX-2,1,yda+2,config.col[4]|64);
  #endif
 #endif

 putchn(' '       ,MAXX-2,1,yda+1,config.col[1]);

 putstr(sstr,0,3,yda,config.col[4]);
// putstr2(sstr);
 buffer=instr; lbuf=strlen(instr);
 ly=yda+1; x=lbuf; vsetcur(x+1,ly);
 first=1;
// putstr(instr,0,1,yda+1,config.col[17]);
 putstr2(instr,1);

 crtdialog=this;

 ret=looptst();

 crtdialog=0;

 buffer[lbuf]='\0';

 if ((dialof&512)==0)
 {
  vrest(save);
  vrestcur(cx,cy);
 }

 dialof&=65536; // redo dialog SE sono

// mutex--;
 return(ret-1); // 0:enter, 1:esc, 2:tab, 3:hexopen, 4:openall
}

/***************************************************************************/
// scrive stringa interna

void dialog::putstr2(char *str2,int evi)
{
 int i,j;

 short int *vidbuf;

 vidbuf=vbuf+config.maxx*ly+1;
 i=strlen(str2);
 if (config.ansi) astatok(ly,ly,1,1+i);
 for (j=0; j<i; j++)
 {
  int col;
  if (evi) col=config.col[17]; else col=config.col[1];
  unsigned char ch=str2[j];

  if ((ch&0xff)<' ') { col=config.col[16]; ch+='a'-1; }
  else
  if ((ch&0x7f)<' ') { col=config.col[16]; ch+='a'-1-128; }

  *(vidbuf++)=(col<<8)|ch;
 }
 putchn(' ',MAXX-2-i,i+1,ly,config.col[1]);
}

/***************************************************************************/
// inserimento carattere

void dialog::execch(int ch)
{
 if (!(flag&FL_SOVR)) // se inserimento
 {
  lbuf=lbuf>?x;
  if (x<lbuf)
  {
   memmove(buffer+x+1,buffer+x,(lbuf-x)<?(MAXLN-x-1));
   *(buffer+lbuf+1)='\0'; putstr2(buffer);
  }
  lbuf++;
 }
 else lbuf=lbuf>?(x+1);
 lbuf=lbuf<?(MAXLN-2);

 flag|=FL_RMODIF; buffer[x]=ch;
 if ((ch&0xff)<' ') { putchr(ch+'a'-1,x+1,ly,config.col[16]); }
 else
 if ((config.ansi>0 && config.ansi<128) && (ch&0x7f)<' ')
 { putchr(ch+'a'-1-128,x+1,ly,config.col[16]); }
 else putchr(ch,x+1,ly,config.col[1]);
 execcmd(131);
}

/***************************************************************************/
// esecuzione comando

int dialog::execcmd(int cmd)
{
 int ex;

 ex=0;

 switch(cmd)
 {
  case 15 : mouse.exec=15;
  case 12 : ex=2; break; // escape, abort

  case 32 : // clip paste
  case 34 : // clip paste & del
   if (clip.edtr && clip.edtr->rtot()>0)
   {
    clip.edtr->nseek(clip.edtr->rtot()-1); // vado all'ultimo

    int l=clip.edtr->rlung()<?MAXLN-lbuf-1;
    int p=lbuf<?x;

    memmove(buffer+p+l,buffer+p,lbuf-p);
    memcpy(buffer+p,clip.edtr->rdati(),l);
    lbuf+=l; x+=l; buffer[lbuf]='\0';
    putstr2(buffer);
    flag|=FL_RMODIF;
    vsetcur(x+1,ly);
    if (cmd==34) clip.edtr->remove();
   }
  break;
  case 91 : if (filespc==1) ex=5; break;  // se richiesto, open all
  case 114 : if (filespc==1) ex=4; break; // se richiesto, open binary
  case 118 : if (filespc==1) ex=6; break; // se richiesto, open readonly
  case 130 : // Canc - delete
   if (x<lbuf)
   {
    memmove(buffer+x,buffer+x+1,lbuf-x-1);
    lbuf--; buffer[lbuf]='\0';
    putstr2(buffer);
    flag|=FL_RMODIF;
   }
  break;
  case 131 : // freccia destra
   if (x<MAXLN-2) { x++; vsetcur(x+1,ly); }
  break;
  case 132 : // freccia sinistra
   if (x>0) { x--; vsetcur(x+1,ly); }
  break;
  case 133 : // go to end of line
   x=lbuf;
   vsetcur(x+1,ly);
  break;
  case 134 : // go to start of line
   if (x>0) { x=0; vsetcur(x+1,ly); }
  break;
  case 135 : // Backspace
   if (x<=lbuf || lbuf==0)
   {
    if (x>0)
    {
     memmove(buffer+x-1,buffer+x,lbuf-x+1);
     buffer[--lbuf]='\0'; x--;
     putstr2(buffer);
     flag|=FL_RMODIF;
     vsetcur(x+1,ly);
    }
   }
   else execcmd(132); // freccia sinistra
  break;
  case 136 : ex=1; break; // Enter - exit ok
  case 137 : // erase line
  {
   int i;
   for (i=0; i<lbuf; i++) buffer[i]=' ';
   lbuf=0;
  }
  break;
  case 138 : // toggle insert/overwrite
   if (flag&FL_SOVR) flag=flag&(FL_SOVR^0xffff);
   else flag|=FL_SOVR;
  break;
  case 139 : // set insert
   flag=flag&(FL_SOVR^0xffff);
  break;
  case 141 : // inscode
  {
   int t;
   do
   {
    t=gettst()&0xff;
    if (t!=13) execch(t);
   } while (t!=13);
  }
  break;
 }
 return(ex);
}

/***************************************************************************/
// smisto caratteri/comandi

int dialog::looptst(void )
{
 int ret,d;

 do
 {
  ret=0;
  d=0;
  switch(exectst(&d))
  {
   case 1 :
    if (d==0x09 && tabexit) return(3);// se tab e richiesto, esco

    if (first)
    {
     first=0; putchn(' ',lbuf,1,ly,config.col[1]);
     lbuf=0; x=lbuf; vsetcur(x+1,ly);
    }
    execch(d);
   break;
   case 2 :
    if (first)
    {
     if (d==32 || d==34) // se clip paste e first, cancello i char vecchi
     {
      putchn(' ',lbuf,1,ly,config.col[1]);
      lbuf=0; x=lbuf; vsetcur(x+1,ly);
     }
     else // mantengo la vecchia stringa
     {
      putstr(buffer,lbuf,1,ly,config.col[1]);
     }
     first=0;
    }
    ret=execcmd(d);
   break;
  }
 } while (!ret);

 return(ret);
}

/***************************************************************************/

/***************************************************************************/
/* XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX */
/***************************************************************************/

#define FNSEP  '\x8c'
#define FNSEP2 '\x8d'

/***************************************************************************/
/* selez.cc - gestione selezione da una lista di elementi */

#define ROW_TO_SCROLL rowtoscr
#define COL_TO_SCROLL config.coltoscroll
#define MAXX2 config.maxx

/***************************************************************************/
// scrivo stringa interna

void selez::putstr2(char *str2,int l,int y,int col)
{
 char st[512];
 int i;

 if (l>500) l=500;

 for (i=0; i<l; i++) if (str2[i]!=FNSEP2) st[i]=str2[i]; else st[i]=' ';
 st[i]='\0';

 putstr(st,l,xda+1,yda+1+y,col);
 putchn(' ',xa-xda-l-1,l+xda+1,yda+y+1,col);
}

/***************************************************************************/
// posiziono cursore

void selez::setcur(int x,int yy)
{ vsetcur(x+xda,yy+yda); }

/***************************************************************************/

int selez::execch(int ch)
{
 int scy=cy;
 int found=0;
 int fix;
 fix=0;
 ch=toupper(ch);

 for (int i=rtot(); i>0; i--)
 {
  next(); if (!fix) { if (cy<(ya-yda-2)) cy++;  else { cy=(ya-yda)/2; fix=1; } }
  char *p=rdati();
  if (tipo<2)
  {
   if (rlung()>0 && (toupper(*p)==ch || (*p=='/' && toupper(*(p+1))==ch)))
   { found=1; break; }
  }
  else
  {
   if (rlung()>0 && (toupper(p[strlen(p)+1])==ch)) { found=1; break; }
  }
 }

 if (found==0) cy=scy;
 putpag();

 if (found && tipo==2) return(execcmd(131));
 return(0);
}

/***************************************************************************/
// put pagina interna

void selez::putpag(void)
{
 elem *scorr;
 int y;

 scorr=corr; y=cy;
 while (irprev(scorr) && y>0) { scorr=irprev(scorr);/*=scorr->prec;*/ y--; }
 if (irprev(scorr)) cy=0;
 putres(0,scorr);
}

/***************************************************************************/
// put resto interna

void selez::putres(int y,elem *scorr)
{
 do
 {
  if (scorr!=corr) putstr2((char *)irdati(scorr),strlen((char *)irdati(scorr))/*irlung(scorr)*/,y,config.col[7]);
  else
  {
   putstr2((char *)irdati(scorr),strlen((char *)irdati(scorr))/*irlung(scorr)*/,y,config.col[8]); cy=y;
   setcur(1,y+1);
  }
  y++; scorr=irnext(scorr);
 } while (y<=(ya-yda-2) && scorr!=0);
 while (y<=(ya-yda-2)) putstr2("",0,y++,config.col[7]);
}

/***************************************************************************/
// eseguo i comandi

int selez::execcmd(int cmd)
{
 int ex;

 ex=0;

 switch(cmd)
 {
  case 1 : // frecciagiu'
   if (crnext()) // se esiste la prossima riga
   {
    next();//corr=corr->pros; lcorr++;
    if (cy<(ya-yda-2)) cy++;
    putpag();
   }
  break;
  case 2 : // frecciasu'
   if (crprev()) // se esiste la riga precedente
   {
    prev(); // corr=corr->prec; lcorr--;
    if (cy>0) cy--;
    putpag();
   }
  break;
  case 3 : // pagina giu'
   if (crnext()) // se esiste almeno la prossima riga
   {
    int y;

    y=ya-yda-2;
    while (crnext() && y>0) { next();/*corr=corr->pros; lcorr++;*/ y--; }
    putpag();
   }
  break;
  case 4 : // pagina su
   if (crprev()) // se esiste la riga precedente
   {
    int y; // row to scroll calcolati

    y=ya-yda-2;
    while (crprev() && y>0) { prev();/*corr=corr->prec; lcorr--;*/ y--; }
    if (!crprev()) cy=0;

    putpag();
   }
  break;
  case 134:
  case 5 : // goto start of file
   if (crprev())
   {
    corr=prima; lcorr=0; cy=0;
    putpag();
   }
  break;
  case 133:
  case 6 : // goto end of file
   if (crnext())
   {
    corr=ulti; lcorr=lulti;
    cy=ya-yda-2;
    putpag();
   }
  break;
  case 7 : // goto start of screen
   if (cy>0)
   {
    while (crprev() && cy>0) { prev();/*corr=corr->prec; lcorr--;*/ cy--; }
    if (!crprev()) cy=0;
    putpag();
   }
  break;
  case 8 : // goto end of screen
   if (cy<ya-yda-2)
   {
    while (crnext() && cy<ya-yda-2) { next();/*corr=corr->pros; lcorr++;*/ cy++; }
    putpag();
//  putpag();
   }
  break;
  case 15 : mouse.exec=15;
  case 12 : ex=2; break; // escape
  case 91 : if (tipo==1) ex=3; break; // open all
  case 114: if (tipo==1) ex=4; break; // open binary
  case 118: if (tipo==1) ex=5; break; // open readonly
  case 131: // freccia destra
  case 136: // enter
  if (tipo<2) { if (cmd==136) ex=1; }
  else // menu
  {
   char *p=rdati();

   p+=strlen(p)+1; // stringa+zero, poi c'e' quick,sub/cmd,number

   if (*(p+1)==1) // vado ad un'altro menu
   {
    int x,y;
    vsavecur(&x,&y);
    int sb;
    unsigned char *up=(unsigned char *)p;

    sb=(*(up+2))|((*(up+3))<<8)|((*(up+4))<<16)|((*(up+5))<<24);

    if (gomenu(sb,xa+1)==1) ex=2; // propagazione dell'escape
    crtselez=this;
    vrestcur(x,y);
   }
   else // esecuzione comando
   {
    if (cmd==136) // se e' un enter, eseguo il comando
    {
     int sb;
     unsigned char *up=(unsigned char *)p;

     sb=(*(up+2))|((*(up+3))<<8)|((*(up+4))<<16)|((*(up+5))<<24);

     int d=sb+1; // avoid zero
     exectst(&d);
     ex=2; // exit
    }
   }
  }
  break; // Enter
  case 132: if (tipo==2) ex=1; break; // freccia sinistra
  case 194 : // m_gotowin
  {
   mouse.togox=mouse.x; mouse.togoy=mouse.y; mouse.win=1;
  } break;
  case 195 : // m_gotozed
  {
   mouse.togox=mouse.x; mouse.togoy=mouse.y; mouse.win=0;
  } break;
 }
 return(ex);
}

/***************************************************************************/
// costruttore : inizializzo

selez::selez(char *str)
{
 strcpy(title,str);
}

/***************************************************************************/
// lancio la selezione

int selez::goselez(char *str,int y,int typ)
{
 int ret,i,l;
 svsave *save;
 int crx,cry;

 vsavecur(&crx,&cry);
// mutex++;
 if (dialof&(8192|65536)) dialof=(8192|256)|(dialof&65536);
 else dialof=256|(dialof&65536);

 tipo=typ;

 corr=prima; lcorr=0; l=0;

 for (i=0; i<rtot(); i++) { l=l>?strlen(rdati());/*rlung();*/ next(); }

 l=l>?strlen(title);

 corr=prima; lcorr=0;

 if (tipo<2) { xda=(config.maxx-l)/2; yda=y; }
 else { xda=y; yda=0; }

 if (winn>0 && tipo==2) yda=win[cwin].myda;

 xa=xda+l+1;

 if (xa>=config.maxx)
 {
  xa=config.maxx-1; xda=xa-l-1;
  if (xda<0)
  {
   confirm("Selector too large ! hit ESC",0,yda,config.col[5]);
   return(1);
  }
 }

 l=rtot();

 ya=yda+l+1; l=config.maxy-1;

 if (ya>l) { yda-=ya-l; ya=l; }
 if (yda<0) yda=0;

 save=vsave(yda,ya);

 #ifdef MSDOS
  putchr('\311',xda,yda,config.col[9]);
  putchr('\273',xa ,yda,config.col[9]);
  putchr('\310',xda,ya,config.col[9]);
  putchr('\274',xa ,ya,config.col[9]);

  putchn('\315',xa-xda-1,xda+1,ya,config.col[9]);
  putchn('\315',xa-xda-1,xda+1,yda,config.col[9]);
  for (i=yda+1; i<ya; i++)
  {
   putchr('\272',xda,i,config.col[9]);
   putchr('\272',xa ,i,config.col[9]);
  }
 #else
  #ifndef X11
   if (config.ansi!=3)
   {
    putchr('/',xda,yda,config.col[9]);
    putchr('\\',xa ,yda,config.col[9]);
    putchr('\\',xda,ya,config.col[9]);
    putchr('/',xa ,ya,config.col[9]);

    putchn('-',xa-xda-1,xda+1,ya,config.col[9]);
    putchn('-',xa-xda-1,xda+1,yda,config.col[9]);
    for (i=yda+1; i<ya; i++)
    {
     putchr('|',xda,i,config.col[9]);
     putchr('|',xa ,i,config.col[9]);
    }
   }
   else
   {
    putchr('\311',xda,yda,0x11);
    putchr('\273',xa ,yda,0x11);
    putchr('\310',xda,ya,0x11);
    putchr('\274',xa ,ya,0x11);

    putchn('\315',xa-xda-1,xda+1,ya,0x11);
    putchn('\315',xa-xda-1,xda+1,yda,0x11);
    for (i=yda+1; i<ya; i++)
    {
     putchr('\272',xda,i,0x11);
     putchr('\272',xa ,i,0x11);
    }
   }
  #else
   putchr(1,xda,yda,config.col[9]|64);
   putchr(2,xa ,yda,config.col[9]|64);
   putchr(3,xda,ya,config.col[9]|64);
   putchr(4,xa ,ya,config.col[9]|64);

   putchn(8,xa-xda-1,xda+1,ya,config.col[9]|64);
   putchn(7,xa-xda-1,xda+1,yda,config.col[9]|64);
   for (i=yda+1; i<ya; i++)
   {
    putchr(5,xda,i,config.col[9]|64);
    putchr(6,xa ,i,config.col[9]|64);
   }
  #endif
 #endif

 putstr(title,0,xda+(xa-xda-strlen(title))/2+1,yda,config.col[9]);

 cy=0; corr=prima; lcorr=0;
 putpag();

 crtselez=this;

 ret=looptst();

 crtselez=0;

 memcpy(str,rdati(),rlung()); str[rlung()]='\0';

 if ((dialof&512)==0)
 { vrest(save); vrestcur(crx,cry); }
 else { delete save->buf; delete save; }

// mutex--;

 if (dialof&512 && dialof&8192) dialof=2048|(dialof&65536);
 else dialof&=65536;

 return(ret-1); // 0:enter, 1:esc, 2:openall
}

/***************************************************************************/
// smisto comandi/caratteri

int selez::looptst(void )
{
 int ret,d;

 do
 {
  ret=0;
  d=0;
  switch(exectst(&d))
  {
   case 1 : ret=execch(d); break;
   case 2 : ret=execcmd(d); break;
  }
 } while (!ret);

 return(ret);
}

/***************************************************************************/
/* XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX */
/***************************************************************************/

// pezzi usati in MAIN & simila, richiesta files & c

/***************************************************************************/
/***************************************************************************/
// controlla se la string verifica pattern
// funzione prelevata dai sorgenti di "ash", reindentata e senza goto

int pmatch(char *pattern,char *string)
{
 char *p,*q,c;

 p=pattern;
 q=string;

 for (;;)
 {
  switch (c=*p++)
  {
   case '\0':
   {
    if (*q!='\0') return(0);
    return(1);
   }
   case '?': if (*q++=='\0') return 0; break;
   case '*':
   {
    c=*p;
    if (c!='?' && c!='*' && c!='[')
    {
     while (*q!=c)
     {
      if (*q=='\0') return 0;
      q++;
     }
    }
    do
    {
     if (pmatch(p,q)) return 1;
    } while (*q++!='\0');
    return 0;
   }
   break;
   case '[':
   {
    char *endp;
    int invert, found,dobr=0;
    char chr;

    endp=p;
    if (*endp=='!') endp++;
    for (;;)
    {
     if (*endp=='\0') // no matching ]
     {
      if (*q++!=c) return 0;
      dobr=1; break; // esco totale dal case
     }
     if (*++endp==']') break;
    }
    if (dobr) break;
    invert=0;
    if (*p=='!') { invert++; p++; }
    found=0;
    chr=*q++;
    c=*p++;
    do
    {
     if (*p=='-' && p[1]!=']')
     {
      p++;
      if (chr>=c && chr<=*p) found=1;
      p++;
     }
     else
     {
      if (chr==c) found=1;
     }
    } while ((c=*p++)!=']');
    if (found==invert) return 0;
   }
   break;

   default:
    if (*q++!=c) return 0;
   break;
  }
 }
// if (*q!='\0') return(0);
// return(1);
}


/***************************************************************************/

void cleanfn(char *fn,int addpath)
{
 char *p;

 #ifdef MSDOS
  if (addpath && *(fn+1)!=':')
  {
   memmove(fn+2,fn,strlen(fn)+1);
   *fn=getdisk()+'a';
   *(fn+1)=':';
  }
  if (addpath && *(fn+2)!='/')
  {
   char tmp[256];
   strcpy(tmp,fn+2);
   _getcwd(fn,510,*fn);
   if (tmp[0]) { strcat(fn,"/"); strcat(fn,tmp); }
  }
 #else
  if (addpath && fn[0]!='/')
  {
   char tmp[256];

   strcpy(tmp,fn);
   _getcwd(fn,512);
   if (tmp[0]) { strcat(fn,"/"); strcat(fn,tmp); }
  }
 #endif

 // tolgo "//"

 while((p=strstr(fn,"//"))!=0) strcpy(p,p+1);

 // tolgo "/./" "/. " "/.\0"  "/../" "/.. " "/..\0"

 int i,l=strlen(fn);

 for (p=fn,i=0; i<l; i++,p++)
 {
  if (*p=='/' && *(p+1)=='.' && (*(p+2)=='/' || *(p+2)==FNSEP || *(p+2)=='\0'))
  {
   if (p==fn && *(p+2)!='/') { strcpy(p+1,p+2); p-=1; l-=1; i-=1; }
                        else { strcpy(p  ,p+2); p-=2; l-=2; i-=2; }
  }

  if (*p=='/' && *(p+1)=='.' && *(p+2)=='.' &&
      (*(p+3)=='/' || *(p+3)==FNSEP || *(p+3)=='\0'))
  {
   char *pp=p;

   if (pp>fn)
   {
    char sv=fn[0];
    fn[0]='\0';

    for (pp=p-1; (*pp!='\0' && (*pp!='/' && *pp!=FNSEP)); pp--);
    fn[0]=sv;
   }

   if (*pp=='/')
   {
    if (pp==fn) strcpy(pp+1,p+2); else strcpy(pp,p+3);

    p=fn; l=strlen(fn); i=0; // troppo complicato da calcolare giusto :)
   }
   // else pathname errato sarebbe da togliere dall'elenco !
  }
 }

 // tolgo le / alla fine delle directory SE non e' la root

 l=strlen(fn);

 for (p=fn,i=1; i<l; i++,p++)
 {
  #ifdef MSDOS
   if ((*(p-1)!=FNSEP && *(p-1)!=':') && *p=='/' && *(p+1)==FNSEP)
  #else
   if (*(p-1)!=FNSEP && *p=='/' && *(p+1)==FNSEP)
  #endif
  {
   strcpy(p,p+1); p--; i--; l--;
  }
 }
 #ifdef MSDOS
  if (l>1 && (fn[l-2]!=FNSEP && fn[l-2]!=':') && fn[l-1]=='/' && fn[l]=='\0')
   fn[l-1]='\0';
 #else
  if (l>1 && fn[l-2]!=FNSEP && fn[l-1]=='/' && fn[l]=='\0') fn[l-1]='\0';
 #endif
}

/***************************************************************************/

void addname(char **str,char *toadd,int *maxlen)
{
 if (*str==0) { *maxlen=1024; *str=new char[*maxlen]; *str[0]='\0'; }
 if (strlen(toadd)+strlen(*str)+2>(unsigned int)*maxlen)
 {
  char *np;

  np=(char *)realloc(*str,*maxlen+1024);
  if (np) { *str=np; *maxlen+=1024; }
 }

 if (strlen(toadd)+strlen(*str)<(unsigned int)*maxlen)
 {
  char sepadd[2]={FNSEP,0};

  if (*str[0]) strcat(*str,sepadd);
  strcat(*str,toadd);
 }
}

/***************************************************************************/
// richiesta tramite dialog di un valido filename con navigazione
// ret: char *:OK 0:abort (esc) (char * to be deleted)

char *askfilename(int argc,char *argv[],int yda,int domanda,int *special)
{
 char curcwd[512];
 char comdir[512];
 char *files;
 int maxfiles,i;
 char *save=0;
 int openall=0;

 comdir[0]='\0'; *special=0;

 if (winn>0)
 {
  char *p;
  strcpy(curcwd,win[cwin].edtr->filename);
  p=strrchr(curcwd,'/');
  if (p) { *p='\0'; _chdir(curcwd); }
 }

 maxfiles=1024; files=new char[maxfiles]; files[0]='\0';

 // mi copio la stringa di partenza
 if (argc>1) for (i=1; i<argc; i++) addname(&files,argv[i],&maxfiles);

 _getcwd(curcwd,512); // salvo il path corrente

 do
 {
  int err=0;
  int forcelist=0;

  if (save)
  {
   restline(yda,save);
   okstatus(yda);
   delete save;
   save=0;
  }

  // ho stringa, se nulla devo chiedere ed espandere

  if (*files=='\0') // stringa nulla ?
  {
   dialog wow;
   char str[100];
   str[0]='\0';

   _chdir(curcwd); // ritorno al path corrente
   switch(domanda)
   {
    case 1 : err=wow.ask("File to open/create:",yda,str,2); break;
    case 2 : err=wow.ask("File to load in clipboard:",yda,str); break;
    case 3 : err=wow.ask("Filename to save clipboard:",yda,str); break;
    case 4 : err=wow.ask("Change Filename to:",yda,str); break;
    case 5 : err=wow.ask("Config file to add:",yda,str); break;
   }

   if (err==3) { *special=2; err=0; } // open in hex mode!
   if (err==4) { openall=1; err=0; } // open all resulting file!
   if (err==5) { *special=4; err=0; } // open in readonly

   if (err) { delete files; return(0); }
   while (str[0]==' ' || str[0]=='\t') strcpy(str,str+1);
   int i=strlen(str);
   while (i>0 && (str[i]==' ' || str[i]=='\t' || str[i]=='\0')) i--;
   str[i+1]='\0';

   if (str[0]=='<') { strcpy(files,str); return(files); }

   strcpy(files,str);

   while (files[0]==' ' || files[0]=='\t') strcpy(files,files+1);
   i=strlen(files);
   while (i>0 && (files[i]==' ' || files[i]=='\t' || files[i]=='\0')) i--;
   files[i+1]='\0';

   for (i=0; i<(int)strlen(files); i++)
   {
    if (files[i]==' ')
    {
     if (files[i-1]=='\\') strcpy(files+i-1,files+i);
                      else files[i]=FNSEP;
    }
    if (files[i]=='\\' && files[i+1]=='\0') files[i]=' ';
   }
  }

  save=new char[config.maxx*2+2];
  saveline(yda,save);

  putstr("Reading directory...     ",25,0,yda,config.col[6]);
  vsetcur(0,0);
  okstatus(yda);

  if (files[0]=='\0') { files[0]='.'; files[1]='\0'; }

  #ifdef MSDOS
  {
   int i,t=strlen(files);
   for (i=0; i<t; i++) if (files[i]=='\\') files[i]='/';
   strlwr(files);
  }
  #endif

  if ((strchr(files,'*')!=0) || (strchr(files,'?')!=0) ||
      (strchr(files,'['))!=0) forcelist=1;

  #ifndef MSDOS

  // espando tutti i ~

  {
   char *p;
   char *h=getenv("HOME");

   if (h)
   {
    while ((p=strchr(files,'~')))
    {
     if ((int)(strlen(files)+strlen(h)+10)>maxfiles)
     {
      maxfiles+=1024; files=(char *)realloc(files,maxfiles);
     }
     memmove(p+strlen(h)-1,p,strlen(p)+1);
     memcpy(p,h,strlen(h));
    }
   }
  }

  #endif

  // append $PWD ai file che non cominciano per '/'

  #ifdef MSDOS
  {
   char cwd[512]; // CON slash finale
   char *p=files;
   char dr=0;

//   _getcwd(cwd,512);
//   strcat(cwd,"/");

   while(p && *p)
   {
    if (*(p+1)!=':')
    {
     if ((strlen(files)+2)>(unsigned int)maxfiles)
     {
      char *np;

      np=(char *)realloc(files,maxfiles+1024);
      if (np) { p=(p-files)+np; files=np; maxfiles+=1024; } else break;
     }
     memmove(p+2,p,strlen(p)+1);
     *p=getdisk()+'a';
     *(p+1)=':';
    }

    if (*(p+2)!='/')
    {
     if (*p!=dr)
     {
      dr=*p;
      _getcwd(cwd,512,*p);
      int l=strlen(cwd);

      if ((*(cwd+l-1)!='/')) { cwd[l]='/'; cwd[l+1]='\0'; }
     }

     if (strlen(files)+strlen(cwd)>(unsigned int)maxfiles)
     {
      char *np;

      np=(char *)realloc(files,maxfiles+1024);
      if (np) { p=(p-files)+np; files=np; maxfiles+=1024; } else break;
     }
//     p[strlen(cwd)+strlen(p)]='\0';
     memmove(p+strlen(cwd),p+2,strlen(p)+1-2); // meno il drive
     memcpy(p,cwd,strlen(cwd));
    }
    p=strchr(p,FNSEP); if (p) p++;
   }
  }
  #else
  {
   char cwd[512];
   char *p=files;

   _getcwd(cwd,512);
   strcat(cwd,"/");

   int l=strlen(cwd);
   if ((*(cwd+l-1)!='/')) { cwd[l]='/'; cwd[l+1]='\0'; }

   while(p && *p)
   {
    if (*p!='/')
    {
     if (strlen(files)+strlen(cwd)>(unsigned int)maxfiles)
     {
      char *np;

      np=(char *)realloc(files,maxfiles+1024);
      if (np) { p=(p-files)+np; files=np; maxfiles+=1024; } else break;
     }
     p[strlen(cwd)+strlen(p)]='\0';
     memmove(p+strlen(cwd),p,strlen(p));
     memcpy(p,cwd,strlen(cwd));
    }
    p=strchr(p,FNSEP); if (p) p++;
   }
  }
  #endif

  cleanfn(files,0);

  // append "/*" alle directory

  {
   char *p=files;

   while(p && *p)
   {
    char *rest;
    struct stat st;
    int err;

    rest=strchr(p,FNSEP); if (rest) *rest='\0'; // isolo il nome

    err=stat(p,&st);

    #ifdef MSDOS
     if (err==0)
      if ((strchr(p,'*')!=0) || (strchr(p,'?')!=0) || (strchr(p,'['))!=0)
       err=1;
     // incredibile, il dos rifiuta un chdir("/uti/./");
     // e non da' errore se stat("d:/uti/*"); !!!!!!!!!!!
    #endif

    if (S_ISDIR(st.st_mode) && err==0) // e' una directory ?
    {
     forcelist=1;

     p+=strlen(p)-1; // punto all'ultimo carattere

     if ((rest?strlen(rest+1):0)+(p-files)+3<(unsigned int)maxfiles)
     {
      if (*p=='/')
      {
       if (rest) { memmove(rest+2,rest+1,strlen(rest+1)+1); rest++; }
       *(p+1)='*'; *(p+2)='\0'; p++;
      }
      else
      {
       if (rest) { memmove(rest+3,rest+1,strlen(rest+1)+1); rest+=2; }
       *(p+1)='/'; *(p+2)='*'; *(p+3)='\0'; p+=2;
      }
     }
    }
    if (rest) { *rest=FNSEP; p=rest+1; } else p=0;
   }
   int i=strlen(files);
   if (i>0) if (files[i-1]==FNSEP) files[i-1]='\0'; // che non finisca in FNSEP
  }

 // espando i metacaratteri *, ?, e [] nei filename

  {
   int err=0;
   int pt=0;
   char *p=files;

   while((p=strchr(files,'*')) || (p=strchr(files,'?')) ||
         ((p=strchr(files,'[')) && strchr(files,']')))
   {
    pt=1;
    char work[512];
    char *da,*to;
    int i;

    for (da=p; da>files && *da!=FNSEP; da--); // cerco l'inizio del filename
    if (*da==FNSEP) da++; // potrebbe non essere il primo

    to=da; // copio & cerco l'inizio del successivo
    for (i=0; *to!=FNSEP && *to!='\0'; i++,to++) work[i]=*to;
    work[i]='\0';
    if (*to==FNSEP) to++; // potrebbe non essere l'ultimo

    { // espansione di work
     DIR *sdir=0;
     struct dirent *entry;
     char *patt;
     char dir[512];

     strcpy(dir,work);

     char *p2=strrchr(work,'/');

     if (p2)
     {
      patt=p2+1;
      strcpy(dir,work);

      p2=strrchr(dir,'/');
      #ifdef MSDOS
       if (p2==dir || (dir[1]==':' && p2==dir+2))
        *(p2+1)='\0'; else *p2='\0'; // check if root !
      #else
       if (p2==dir) *(p2+1)='\0'; else *p2='\0'; // check if root !
      #endif
     } else { dir[0]='.'; dir[1]='\0'; patt=work; }

     {
      char savepath[512];

      _getcwd(savepath,512);
      err=_chdir(dir);

      if (err==0)
      {
       sdir=opendir(".");
       strcpy(comdir,dir);
      }

      if (err==0 && sdir!=0)
      {
       while((entry=readdir(sdir))!=0)
       {
        if (pmatch(patt,entry->d_name)) // corrisponde ?
        {
         struct stat st;
         stat(entry->d_name,&st);

         if (S_ISREG(st.st_mode))
         {
          char file[512];
          strcpy(file,dir);
          if (file[strlen(file)-1]!='/') strcat(file,"/");
          strcat(file,entry->d_name);
          char *ex=files;
          addname(&files,file,&maxfiles);
          if (ex!=files)
          { p=(p-ex)+files; da=(da-ex)+files; to=(to-ex)+files; }
         }
        }
       }
      }
      if (sdir) closedir(sdir);
      _chdir(savepath);
     }
    }

    strcpy(da,to);
   }
   if (err)
   {
    confirm("Path not found, hit ESC",0,yda,config.col[5]);
    continue; // abort & redo
   }
   while (files[0]==FNSEP) strcpy(files,files+1);
  }

  // ho stringa espansa, guardo i casi limite un file o una directory

  if (!strchr(files,FNSEP) && files[0] && forcelist==0) // caso di un file solo
  {
   cleanfn(files,1);

//   if (_chdir(files)==0) files[0]='\0'; // era una directory
//   else
   {
    int err=0;

    char *p=strrchr(files,'/');

    #ifdef MSDOS
     if (p)
     {
      char sc=files[3];

      if (p==files+2) files[3]='\0'; else *p='\0';
      if (_chdir(files)!=0) err=1;
      else { files[3]=sc; strcpy(files,p+1); }
     }
    #else
     if (p)
     {
      char sc=files[1];

      if (p==files) files[1]='\0'; else *p='\0';
      if (_chdir(files)!=0) err=1;
      else { files[1]=sc; strcpy(files,p+1); }
     }
    #endif

    if (err!=0)
    {
     confirm("Path not found, hit ESC",0,yda,config.col[5]);
     files[0]='\0';
     continue; // abort & redo
    }
    if (save)
    {
     restline(yda,save);
     okstatus(yda);
     delete save;
     save=0;
    }

    return(files); // file da aprire
   }
  }

  if (files[0])
  {
   // adesso cerco la directory comune a tutti i file

   {
    char *p1,*p=files;

    p1=strchr(files,FNSEP);
    if (p1) *p1='\0';
    strcpy(comdir,files);
    if (p1) *p1=FNSEP;

    while(p && *p)
    {
     for (i=0; comdir[i]!='\0' && p[i]!='\0'; i++)
      if (comdir[i]!=p[i]) comdir[i]='\0';
     comdir[i]='\0';

     p=strchr(p,FNSEP); if (p) p++;
    }

    if (comdir[0]) *(strrchr(comdir,'/')+1)='\0';
   }

   // adesso tolgo le directory solitarie e i file non esistenti

   {
    char *p=files;

    while(p && *p)
    {
     char *rest;
     struct stat st;
     int err,ok=1;

     rest=strchr(p,FNSEP); if (rest) *rest='\0'; // isolo il nome

     err=stat(p,&st);

     if (S_ISDIR(st.st_mode) || err!=0) // e' una directory ?
     {
      if (rest) { strcpy(p,rest+1); ok=0; }
      else *p='\0'; // files potrebbe finire in spazio
     }
     if (ok) if (rest) { *rest=FNSEP; p=rest+1; } else p=0;
    }
    int i=strlen(files);
    if (i>0) if (files[i-1]==FNSEP) files[i-1]='\0'; // che non finisca in spazio
   }

   // adesso strippo la comdir da tutti i file

   {
    int ll=strlen(comdir);

    if (ll>0)
    {
     char *p=files;

     while(p && *p)
     {
      strcpy(p,p+ll);

      p=strchr(p,FNSEP); if (p) p++;
     }

     p=comdir+strlen(comdir)-1;

  //   if (*p=='/') *p='\0';

     #ifdef MSDOS
      if (p==comdir || (comdir[1]==':' && p==comdir+2))
       *(p+1)='\0'; else *p='\0'; // check if root !
     #else
      if (p==comdir) *(p+1)='\0'; else *p='\0'; // check if root !
     #endif

     if (comdir[0] && _chdir(comdir)==-1)
     {
      confirm("Path not found, hit ESC",0,yda,config.col[5]);
      files[0]='\0';
      continue; // abort & redo
     }
    }
   }
  }

  if (comdir[0]=='\0') _getcwd(comdir,512);
  if (comdir[0]) _chdir(comdir);

  // adesso cancello tutti i file doppi

  if (files[0])
  {
   char *p=files;

   while(p && *p)
   {
    char *rest;
    int ok=1;
    int todel=0;

    rest=strchr(p,FNSEP); if (rest) *rest='\0'; // isolo il nome

    if (rest)
    {
     char *rest2;
     char *p2=rest+1;

     while(p2 && *p2)
     {
      rest2=strchr(p2,FNSEP); if (rest2) *rest2='\0'; // isolo il nome

      if (strcmp(p,p2)==0) { todel=1; break; }
      if (rest2) { *rest2=FNSEP; p2=rest2+1; } else p2=0;
     }
    }

    if (todel)
     if (rest) { strcpy(p,rest+1); ok=0; }
     else *p='\0'; // files potrebbe finire in spazio

    if (ok) if (rest) { *rest=FNSEP; p=rest+1; } else p=0;
   }
   int i=strlen(files);
   if (i>0) if (files[i-1]==FNSEP) files[i-1]='\0'; // che non finisca in spazio
  }

  // adesso aggiungo tutti i file se files nullo e tutte le directory

  {
   DIR *cdir;
   struct dirent *entry;
   struct stat st;
   int toadd=(files[0]=='\0');

   cdir=opendir(".");

   while((entry=readdir(cdir))!=0)
   {
    char file[512];

    stat(entry->d_name,&st);

    file[0]='\0';
    if (S_ISDIR(st.st_mode)) { strcpy(file,entry->d_name); strcat(file,"/"); }
    else
    if (S_ISREG(st.st_mode) && toadd) strcpy(file,entry->d_name);
    if (file[0]) addname(&files,file,&maxfiles);
   }
   closedir(cdir);
  }

  // controllo che i nomi non siano troppo lunghi e calcolo il nome piu' lungo
  // eventualmente tronco comdir

  int maxlen=0;

  {
   char *p=files;

   while(p && *p)
   {
    char *rest;

    rest=strchr(p,FNSEP); if (rest) *rest='\0'; // isolo il nome

    maxlen=maxlen>?strlen(p);
    if (rest) { *rest=FNSEP; p=rest+1; } else p=0;
   }

   if (maxlen>config.maxx-10)
   {
    confirm("Pathname too long, hit ESC",0,yda,config.col[5]);
    files[0]='\0';
    continue; // abort & redo
   }

   int mm=config.maxx-10<?maxlen*2;

   if ((int)strlen(comdir)>mm)
   {
    strcpy(comdir,comdir+strlen(comdir)-mm);
    comdir[0]='.'; comdir[1]='.'; comdir[2]='.';
   }
  }

  // adesso lancio la scelta all'utente, come minimo ho tutte le directory

  {
   selez slz(comdir);
   char *p=files;
   char str[512];
   int ret;

   while (p && *p)
   {
    char *rest,*p2;
    struct stat st;
    int i;

    rest=strchr(p,FNSEP); if (rest) *rest='\0'; // isolo il nome

    strcpy(str,p);

    p2=strchr(str,'/');
    //(!strchr(str,'/') && !stat(str,&st))
    if (stat(str,&st)==0)
    {
     if (!(p2!=0 && *(p2+1)=='\0'))
     {
      for (i=strlen(str); i<maxlen+1; i++) str[i]=FNSEP2;

      int cl=st.st_size;
      char ch=' ';

      if (cl>999L) { cl/=1024; ch='k'; }
      if (cl>999L) { cl/=1024; ch='M'; }
      if (cl>999L) { cl/=1024; ch='G'; }
      sprintf(str+maxlen+1,"%3i",cl);
      str[maxlen+1+3]=ch;
      str[maxlen+1+3+1]='\0';
     }
     else
     {
      memmove(str+1,str,strlen(str)+1); str[0]='/';
      str[strlen(str)-1]='\0';
     }

     slz.add(str,strlen(str)+1,slz.sseek(str));
    }
    if (rest) { p=rest+1; *rest=FNSEP; } else p=0;
   }

   dialof=8192|(dialof&65536); // indico che sotto c'e' un'altro dialog

   if (save)
   {
    restline(yda,save);
    okstatus(yda);
    delete save;
    save=0;
   }

   if (!openall) ret=slz.goselez(str,yda,1);
   else          ret=2;

   if (ret==0 || ret==3 || ret==4) // singolo & singolo/hex mode/readonly!
   {
    char *p=strchr(str,FNSEP2); // TOBEFIXED!
    if (p) *p='\0';
    strcpy(files,str);
    if (ret==3) *special=2;
    if (ret==4) *special=4;
   }
   else if (ret==1) { files[0]='\0'; continue; } // escape
   else if (ret==2) // open all, tolgo le directory
   {
    if (domanda!=1) { files[0]='\0'; continue; }

    char *p,*pf;

    for (p=files; *p!='\0'; p++)
    {
     if (*p=='/' && (*(p+1)==FNSEP || *(p+1)=='\0'))
     {
      for (pf=p; *pf!=FNSEP && pf!=files; pf--);

      if (*(p+1)=='\0') *pf='\0'; else { strcpy(pf,p+1); p=pf; }
     }
    }
   }

   if (files[0]=='\0') continue;
   if (files[0]=='/') strcpy(files,files+1); else return(files);
  }
 } while (1==1);
}

/***************************************************************************/
// sistemo il filename e controllo se esiste gia' tra gli editor in memoria

editor *chkname(char *name,char *pn)
{
 int i;

 // pulisco il filename
// while (name[0]==' ' || name[0]=='\t') strcpy(name,name+1);
// i=strlen(name);
// while (i>0 && (name[i]==' ' || name[i]=='\t' || name[i]=='\0')) i--;
// name[i+1]='\0';

 strcpy(pn,name);
 cleanfn(pn,1);

 edt.nseek(0);

 for (i=0; i<edt.rtot(); i++)
 {
  char *p2=(*((editor **)edt.rdati()))->rfilename();

  if (!strcmp(pn,p2)) return(*((editor **)edt.rdati()));
  edt.next();
 }
 return(0);
}

/***************************************************************************/

// richiesta tramite dialog di un valido filename
// ret: 1:outofmem, 2:abort, 3 ok ma non caricato, editor *

// se devo aprire un editor, lo alloco e lo aggiungo alla lista degli editor

// se sedt!=0 allora chiedo un nuovo nome per il sedt (non salvo)
// se ya==-1 allora devo caricare il file nella clipboard

editor *askfile(int argc,char *argv[],int yda,int ya,editor *sedt)
{
 char *files=0;
 editor *ret;
 int err;
 int domanda;
 int isspecial=0;
 /*
    =0 normale
    =1 per richiesta help interno, unico caso finora
    =2 per richiesta di file in modo hex-binario
    =4 per richiesta di file in readonly
 */
 if (sedt==(editor *)1) { domanda=5; sedt=0; }
 else
 if (ya==-1) domanda=2;  // err=wow.ask("File to load in clipboard:",yda,str);
 else
 if (sedt==0) domanda=1; // err=wow.ask("File to open/create:",yda,str);
 else
 if (sedt==clip.edtr) domanda=3; // err=wow.ask("Filename to save clipboard:",yda,str);
 else domanda=4;         // err=wow.ask("Change Filename to:",yda,str);

 exectst((int *)1);

 do
 {
  delete files;
  files=askfilename(argc,argv,yda,domanda,&isspecial);
  if (files==0) ret=(editor *)2; else ret=0;
  err=0;

  if (files)
  {
   if (files[0]=='<')
   {
    if (domanda!=1) continue;

    if (strnicmp(files,"<H",2)==0)
    {
     isspecial=1; files[0]='>'; files[1]='H';
    }
    else
    if (strnicmp(files,"<G",2)==0)
    {
     #ifndef UNIX
      strcpy(files,argv0); char *p=strrchr(files,'.'); if (p) strcpy(p,".cfg");
     #else
      #ifdef X11
       strcpy(files,getenv("HOME")); strcat(files,"/.zedxrc");
      #else
       strcpy(files,getenv("HOME")); strcat(files,"/.zedrc");
      #endif
     #endif
    }
    else
    if (strnicmp(files,"<E",2)==0)
    {
     #ifndef UNIX
      strcpy(files,argv0); char *p=strrchr(files,'.'); if (p) strcpy(p,".std");
     #else
      #ifdef X11
       if (access("/etc/zedxrc",R_OK)==0)
        strcpy(files,"/etc/zedxrc"); else strcpy(files,"/usr/local/etc/zedxrc");
      #else
       if (access("/etc/zedrc",R_OK)==0)
        strcpy(files,"/etc/zedrc"); else strcpy(files,"/usr/local/etc/zedrc");
      #endif
     #endif
    }
    else
    {
     if (edt.rtot()>0)
     {
      if (strnicmp(files,"<I",2)==0)
      {
       ret=bufsel(yda);
       if (ret!=0)
       {
        vputstr("Sel Ok",6,yda,config.col[6]);
        ret->orflag(FL_SCRITT);
       }
      }
      else
      if (strnicmp(files,"<C",2)==0)
      {
       int i;
       ret=(editor *)0;

       for (i=0; i<winn; i++)
        if (win[i].edtr==clip.edtr) { ret=(editor *)3; cwin=i; break; }

       if (ret) { vputstr("Sel Ok",6,win[cwin].myda,config.col[6]); }
       else   { ret=clip.edtr; vputstr("Clp Sel",7,yda,config.col[6]); }
       clip.edtr->orflag(FL_SCRITT);
      }
      else
      if (!strnicmp(files,"<D",2))
      {
       int i;
       for (i=0; i<winn; i++)
        if (win[i].edtr==delbuf) { ret=(editor *)3; cwin=i; break; }

       if (ret) { vputstr("Sel Ok",6,win[cwin].myda,config.col[6]); }
       else   { ret=delbuf;  vputstr("DelB Sel",8,yda,config.col[6]); }
       delbuf->orflag(FL_SCRITT);
      }
      else err=14;
     }
     else err=14;
    }
   } // fine gestione "<?"

   if (files[0]!='<') // comprende il caso "<g" ! (perche' il nome e' stato cambiato)

   // comincio a caricare i files

   if (strchr(files,FNSEP)==0) // c'e' un file solo...
   {
    if (domanda==5) // carico il relati config file
    {
     #ifndef X11
      clearscr();
      deinit();
     #endif

     err=0; ret=0;
     if (cconfig(files)!=0)
     {
      #ifndef X11
       output("\n Press ENTER to continue...\n");
       gettst3();
      #endif
     } else ret=(editor *)1;

     exectst((int *)4); // azzero i puntatori eventuali

     #ifndef X11
      preinit();
      init(1);
      clearscr();
     #endif

     #ifdef X11
      for (int i=0; i<MAXCOLORS; i++) config.col[i]=i;
     #endif

     chkwinsize(); // check video
     wup(); win[cwin].edtr->puthdr(3);
    }

    if (domanda==3 || domanda==4) // change filename to oppure save_clip
    {
     char pn[512];
     char ch='y';

     if (chkname(files,pn)) err=11;
     else
     if (access(files,W_OK)!=0) err=10;
     else
     {
      if (domanda==4)
       ch=confirm("Overwrite existing file (y/n) ?","yn",yda,config.col[6]);
      else
       ch=confirm("Overwrite existing file (y/n/a) ?","yna",yda,config.col[6]);

      if ((ch=='y' || ch=='a') && sedt)
      {
       strcpy(sedt->rfilename(),files); sedt->puthdr(16);
       sedt->orflag(FL_MODIF);
       if (ch=='a') sedt->orflag(FL_APPEND);
       ret=(editor *)3;
      }
     }
     if (err==10 && errno==ENOENT)  // semplicemente il file non esisteva
     {
      err=0;
      strcpy(sedt->rfilename(),files); sedt->puthdr(16);
      sedt->orflag(FL_MODIF);
      ret=(editor *)3;
     }
    }
    else
    if (domanda==2) // devo caricare la clipboard
    {
     if (access(files,R_OK)==0)
     {
      clip.edtr->ydaset(yda);
      err=clip.edtr->loadfile(files,0);
      clip.edtr->orflag(FL_SCRITT);
      if (err==0) ret=(editor *)3;
     }
     else err=10;
    }
    else
    if (domanda==1) // devo caricare il file
    {
     editor *sw=0;

     switch(isspecial)
     {
      case 1: // help file
      {
       ret=0;

       edt.nseek(0); // cerco se c'e' gia'

       for (int i=0; i<edt.rtot(); i++)
       {
        char *p2=(*((editor **)edt.rdati()))->rfilename();

        if (p2[0]=='>' && p2[1]=='H') ret=(*((editor **)edt.rdati()));
        edt.next();
       }

       if (!ret)
       {
        ret=new editor(yda,ya);
        err=ret->createhelp();
        if (err==0) { if (edt.add((char *)&ret,sizeof(editor *),3)) err=3; }
        else { delete ret; ret=0; }
        sw=ret;
       }
       else
       {
        int i;

        sw=ret;
        for (i=0; i<winn; i++)
         if (win[i].edtr==ret) { ret=(editor *)3; cwin=i; break; }
        vputstr("Sel Ok",6,win[cwin].myda,config.col[6]);
       }
      } break;
      default:
      {
       char pn[512];
       ret=chkname(files,pn);

       if (!ret)
       { // devo creare un nuovo editor
        ret=new editor(yda,ya);
        err=ret->loadfile(pn,isspecial);
        if (err==0) { if (edt.add((char *)&ret,sizeof(editor *),3)) err=3; }
        else        { delete ret; ret=0; }
        sw=ret;
       }
       else // e' in memoria, controllo se e' a video
       {
        int i;

        sw=ret;
        for (i=0; i<winn; i++)
         if (win[i].edtr==ret) { ret=(editor *)3; cwin=i; i=winn; }
        vputstr("Sel Ok",6,win[cwin].myda,config.col[6]);
       }
      } break;
     }
     if (sw) sw->orflag(FL_SCRITT);
    }
   }

   else

   // ci sono tanti file

   if (domanda==1) // posso caricarli ?
   {
    char pn[512];
    char *next=files;
    editor *lastok=0;

    while(next && *next && err==0)
    {
     char *rest;
     rest=strchr(next,FNSEP); if (rest) *rest='\0'; // isolo il nome

     ret=chkname(next,pn);

     if (ret==0) // non c'e' gia'
     {
      ret=new editor(yda,ya);
      err=ret->loadfile(pn,1);
      ret->orflag(FL_SCRITT);
      if (err==0) if (edt.add((char *)&ret,sizeof(editor *),3)) err=3;
      if (err==0) lastok=ret; else delete ret;
     }
     if (rest) next=rest+1; else next=0;
    }
    ret=lastok; if (ret && err) err+=100;
   }
   else err=13;
  }

  if (err)
  {
   if (err>100) err-=100; else ret=0;
   switch(err)
   {
    case 1:  confirm("Error reading file, hit ESC",0,yda,config.col[5]); break;
    case 3:  confirm(s_oom,0,win[cwin].myda,config.col[5]); ret=(editor *)1; break;
    case 5:  confirm("Error opening file, hit ESC",0,yda,config.col[5]); break;
    case 10: confirm("Error accessing file, hit ESC",0,yda,config.col[5]); break;
    case 11: confirm("FileName already used, hit ESC",0,yda,config.col[5]); break;
    case 13: confirm("Invalid action, hit ESC",0,yda,config.col[5]); break;
    case 14: confirm("Unknow or invalid \"<?\" action, hit ESC",0,yda,config.col[5]); break;
   }
  }
 } while(ret==0);
 if (ret==(editor *)1 || ret==(editor *)2) exectst((int *)2); else exectst((int *)3);
 delete files;
 return(ret);
}

/***************************************************************************/

int gomenu(int start,int xx)
{
 char *p;
 char str[100];
 char quick=0;

 if (config.pmnus==0) return(0);

 if (config.mnus[start].prev<0) strcpy(str,"Menu");
 else strcpy(str,config.strs+config.mnus[config.mnus[start].prev].nome);

 while ((p=strchr(str,'*'))) strcpy(p,p+1);
 while ((p=strchr(str,'_'))) *p=' ';

 selez menu(str);

 int pnt=start;

 while (pnt>-1)
 {
  int l;

  quick=0;
  str[0]=str[1]='\0';
  if (config.mnus[pnt].sub>-1) str[0]='<';
  strcat(str,config.strs+config.mnus[pnt].nome);
  if (config.mnus[pnt].sub>-1) strcat(str,">");

  while ((p=strchr(str,'*'))) { quick=*(p+1); strcpy(p,p+1); }
  while ((p=strchr(str,'_'))) *p=' ';

  l=strlen(str);
  str[l+1]=quick; // quick

  if (config.mnus[pnt].sub>-1)
  {
   int sb=config.mnus[pnt].sub;

   str[l+2]=1;
   str[l+3]=sb&0xff;
   str[l+4]=(sb&0xff00)>>8;
   str[l+5]=(sb&0xff0000)>>16;
   str[l+6]=(sb&0xff000000)>>24;

//   *(int *)(&str[l+3])=config.mnus[pnt].sub;
  }
  else
  {
   int sb=config.mnus[pnt].cmd;

   str[l+2]=0;
   str[l+3]=sb&0xff;
   str[l+4]=(sb&0xff00)>>8;
   str[l+5]=(sb&0xff0000)>>16;
   str[l+6]=(sb&0xff000000)>>24;

//   *(int *)(&str[l+3])=config.mnus[pnt].cmd;
  }

  menu.add(str,strlen(str)+1+6,3);

  pnt=config.mnus[pnt].next;
 }

 dialof=8192|(dialof&65536); // indico che sotto c'e' un'altro dialog

 return(menu.goselez(str,xx,2));  // tipo menu
}

/***************************************************************************/


