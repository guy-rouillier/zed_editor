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

/* editor.cc - gestione del singolo editor/buffer, prima parte*/

#define MAXX config.maxx

#include <errno.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <regex.h>

#ifdef X11
 #include <X11/Xlib.h>
 #include <X11/Xatom.h>
 #include <X11/keysym.h>
#endif

#include "zed.h"

#ifdef MSDOS
 #include <dos.h>
 #include <dpmi.h>
#endif

//#ifdef __sun
// #include <rpc/types.h>
//#endif

#define EOF_STR (config.strs+config.streof)
#define EOF_STR_LEN (strlen(config.strs+config.streof))

static int internal=0;

/***************************************************************************/

void editor::okdelbuf(void)
{
 while(delbuf->rtot()>config.maxdelbuf)
 { delbuf->nseek(0); delbuf->remove(); }
 winchk(delbuf);
}

/***************************************************************************/
// sistemo i valori di selezione blocco

void editor::blockok(void)
{
 if ((block.blk&3)==3)
 {
  if (block.start>block.end)
  {
   int l;

   l=block.start; block.start=block.end; block.end=l;
   l=block.cstart; block.cstart=block.cend; block.cend=l;
  }
  if ((block.type || block.start==block.end) && block.cstart>block.cend)
  { int i; i=block.cstart; block.cstart=block.cend; block.cend=i; }
  if (block.cend==0 && block.start<block.end && !block.type)
  { block.end--; block.cend=1023; }
  //sanity check
  if (block.type && block.cend==1023) block.cend=block.cstart;
 }
}

/***************************************************************************/
// copia la riga corrente nel buffer di riga

void editor::started(void)
{
 if (!binary)
 {
  lbuf=rlung(); soft=softret();
  memcpy(buffer,rdati(),lbuf);
  memset(buffer+lbuf,' ',MAXLINE-lbuf);
 }
 else
 {
  int i;

  memset(buffer,' ',80);
  lbuf=rlung(); soft=0;
  memcpy(buffer+59,rdati(),lbuf);

  unsigned char *bin=(unsigned char *)buffer;

  sprintf((char *)bin,"%08X",lcorr*16); bin[8]=' ';

  if (lbuf==16)
  {
   sprintf((char *)bin+10,"%02X %02X %02X %02X %02X %02X %02X %02X "
            "%02X %02X %02X %02X %02X %02X %02X %02X",
   bin[59+0],bin[59+1],bin[59+2],bin[59+3],bin[59+4],
   bin[59+5],bin[59+6],
   bin[59+7],bin[59+8],bin[59+9],bin[59+10],bin[59+11],
   bin[59+12],bin[59+13],
   bin[59+14],bin[59+15]);

   bin[10+15*3+2]=' ';
  }
  else
  for (i=0; i<lbuf; i++)
  {
   sprintf((char *)bin+10+i*3,"%02X",bin[59+i]); bin[10+i*3+2]=' ';
  }

  lbuf+=59;
 }

 flag|=FL_BUFFER;
}

/***************************************************************************/
// aggiorno la riga corrente con il buffer

void editor::update(void)
{
 int ok;

 if (!(flag&FL_BUFFER)) return;
 ok=0;
// if ((flag&FL_RMODIF) && (!(flag&FL_MODIF))) // se la riga e' stata modif
 if (flag&FL_RMODIF) // se la riga e' stata modif
 {
  if (lbuf!=((rlung()-(binary?59:0))&0x3ff)) ok=1; // controllo che la riga sia uguale
  else ok=memcmp(buffer,rdati(),lbuf);
  if (ok)
  {
   setmodif();
   if (!binary)
    if (delbuf->add(rdati(),rlung(),3)) confirm(s_oom,0,yda,config.col[5]);
  }
  puthdr(4);
 }
 if ((flag&FL_RMODIF) && (flag&FL_MODIF))
 {
//  if (config.tabexpr) tabexp(buffer,&lbuf);
//  if (subst(buffer,lbuf|(rlung()&0xfc00))) confirm(s_oom,0,yda,config.col[5]);

  int okok=list::rlung()&0xfc00;

  if (!binary) subst(buffer,lbuf|okok/*(rlung()&0xfc00)*/);
  else         subst(buffer+59,(lbuf-59)|okok/*(rlung()&0xfc00)*/);
  if (docmode) setsoft(soft);
  puthdr(4);
 }
 flag&=(FL_BUFFER^0xffff);

 if (ok) okdelbuf();
}

/***************************************************************************/
// esegue l'inserimento di un carattere nel testo
// se blocco selezionato, faccio resize del blocco

void editor::execch(int ch)
{
 int cmd=131;
 int r=0;

 if (todraw) { puthdr(3); todraw=0; }
 if (redraw) { redraw=0; putelm(0,0,vidy); } // redraw elemento

 if (readonly) { execcmd(131); return; }

 if (!(flag&FL_BUFFER)) started();

 if (!binary)
 {
  if (!(flag&FL_SOVR) || ch==9) // se inserimento o TAB
  {
   if (block.blk&4 && block.edtr==this && !block.type)
   {
    blockok();
    if (block.start==lcorr && x<block.cstart && block.cstart!=1023)
     block.cstart++;
    if (block.end==lcorr && x<block.cend && block.cend!=1023) block.cend++;
   }
   lbuf=lbuf>?x;
   lbuf++;
   if (x<lbuf) { memmove(buffer+x+1,buffer+x,(lbuf-x-1)<?(MAXLINE-x-1)); }
  }
  else lbuf=lbuf>?(x+1);
  lbuf=lbuf<?(MAXLINE-2);

  flag|=FL_RMODIF; buffer[x]=ch;

  r=1;

  if (ch=='\t' && config.tabexpr)
  {
   int t=config.tabsize-(x%config.tabsize);

   if ((lbuf+t<=1023))
   {
    memmove(buffer+x+t-1,buffer+x,lbuf-x);
    memset(buffer+x,' ',t);
    lbuf+=t-1;
    x+=t-1;
   }
  }

  if (docmode && (config.ww&1)==0 && lbuf>config.rmargin && lbuf==x+1)
  { // wordwrap banale
   int nx=x;

   if (ch==' ') cmd=176; // soft enter
   else
   {
    while (nx>0 && buffer[nx-1]!=' ') nx--;
    if (nx>0)
    {
     int saveflag=flag; // salvo sovrascrittura o no
     x=nx;
     flag=flag&(FL_SOVR^0xffff); // la linea nuova va _inserita_
     execcmd(176); // soft enter
     execcmd(133); // goto end of line
     flag=saveflag;
     r=0;
    }
   }
  }
  else
  if (docmode && (config.ww&1)==1 && lbuf>config.rmargin)
  { // wordwrap tosto
   internal=1;
   if (config.ww&16) execcmd(116); // j_quoterem
   if (config.ww&2) execcmd(78); // j_dejust
   if (config.ww&4) execcmd(76); // j_format
   if (config.ww&8) execcmd(77); // j_justif
   internal=0;
   if (config.ww&16) execcmd(117); // j_quoteres
  }
 }
 else
 {
  if (x>=10 && x<=56 && ((x-10)%3)!=2 && isxdigit(ch))
  {
   int pos=(x-10)/3;

   if (((x-10)%3)==0) buffer[59+pos]=(htoi(ch)<<4)|(buffer[59+pos]&0x0f);
   else               buffer[59+pos]=(htoi(ch)   )|(buffer[59+pos]&0xf0);

   flag|=FL_RMODIF;
   r=1;
  }
  else
  if (x>=59 && x<=74)
  {
   flag|=FL_RMODIF; buffer[x]=ch;
   r=1;
  }
  else if (x<75) execcmd(131);
 }

 if (r)
 {
  putelm(0,0,vidy);
  if (vidy==match.y && match.x>x) match.x++;
  execcmd(cmd);
 }
}

/***************************************************************************/
// controlla se il carattere ch fa parte di una parola o no

int editor::isword(int ch)
{ return(isalpha(ch) || isdigit(ch) || ch=='_'); }

/***************************************************************************/
// allarga il blocco solo se il blocco puo' essere allargato/ristretto

inline void editor::blkcend(int x) { if (block.cend!=1023) block.cend+=x; }

/***************************************************************************/

void editor::gotol(int line)
{
 int sl=lcorr-(vidy-yda-1);

 nseek(line);
 int sl2=lcorr;
 if (sl2<sl) { int tmp=sl2; sl2=sl; sl=tmp; }
 if ((sl2-sl)<(ya-yda)) vidy=yda+(int)(sl2-sl+1);
 else                     vidy=yda+(ya-yda)/2;
}

/***************************************************************************/
/* esegue un comando quasiasi, dallo spostamento del cursore all'editazione
   generica....
   se il comando e' <129 e' un comando di spostamento all'interno del file
   se il comando e' >128 e' un comando di editazione riga*/

int editor::execcmd(int cmd)
{
// static char *sprec=0;
// static char save=0;
 static int px;
 static int plc;
 int ex,rdw;
 int rdda=0,rda=0;

 elem *chkcor=corr;

 if (config.showmatch && match.x!=-1)
 {
  short int *pnt;

  pnt=vbuf+match.y*config.maxx+match.x-vidx;
  *pnt=match.save;

  match.x=-1;
  if (config.ansi) astatok(match.oy,match.oy,match.ox,match.ox);
 }

/* rdw=ReDraw What :
   rdw : 1=putelm(0,vidy); 2=putres(vidy,corr,lcorr); 4=putpag();
   8=setcur(x,vidy); 16=puthdr(8); 32=puthdr(4); 64=copy_cmd;
   128=okdelbuf 256=put da/a 512=recalc. paragraph

   ex=EXit code */

 ex=0; rdw=0;
 if (todraw==1) { puthdr(3); todraw=0; } // se testata da aggiornare
 if (redraw) { redraw=0; putelm(0,0,vidy); }
 if (cmd>128 && (!(flag&FL_BUFFER))) started();
 if (cmd<129 &&   (flag&FL_BUFFER))  update();

 if (binary) // elimino tutti i comandi che non posso dare in binary
 {
  switch(cmd)
  {
   case 135: cmd=132; break; // backspace --> freccia sinistra
   case 176 : // soft enter
   case 136 : // hard enter
   {
    internal=1; rdw|=execcmd(1); internal=0; /* giu */
    vx=x=0; rdw|=8; /* goto SOL */
    cmd=-1; // noop!
   }; break;
   case 30 :  case 31 :  case 32 :  case 33 :  case 34 :  case 39 :  case 40 :
   case 41 :  case 42 :  case 43 :  case 44 :  case 49 :  case 50 :  case 72 :
   case 73 :  case 74 :  case 75 :  case 76 :  case 77 :  case 78 :  case 79 :
   case 80 :  case 82 :  case 83 :  case 84 :  case 85 :  case 86 :  case 87 :
   case 88 :  case 89 :  case 90 :  case 94 :  case 95 :  case 97 :  case 98 :
   case 99 :  case 105 :
   #ifdef X11
    case 106:   case 107:   case 108:   case 109:   case 110:
   #endif
   case 113 :  case 116 :  case 117 :
   case 130 :
   case 138 :  case 139 :  case 140 :
   case 141 :  case 144 :  case 145 :  case 146 :  case 147 :  case 148 :
   case 150 :  case 151 :  case 156 :  case 152 :  case 153 :  case 154 :
   case 155 :  case 161 :  case 162 :  case 163 :  case 164 :  case 165 :
   case 166 :  case 167 :  case 168 :  case 169 :  case 177 :  case 178 :
   case 179 :  case 180 :  case 191 :
   {
    cmd=-1; // noop!
   } break;
  }
 }

 if (readonly) // elimino tutti i comandi che non posso dare in readonly
 {
  switch(cmd)
  {
   case 30: cmd=31; break; // cut --> copy
   case 42: // ripete solo la ricerca!
   {
    if (s_flag&512) cmd=-1;
   } break;

   case 135: cmd=132; break; // backspace --> freccia sinistra
   case 176: case 136: // soft/hard enter
   {
    internal=1; rdw|=execcmd(1); internal=0; /* giu */
    vx=x=0; rdw|=8; /* goto SOL */
    cmd=-1;
   } break;

   case 9: case 32: case 33: case 34: case 35: case 39: case 40: case 44:
   case 75: case 76: case 77: case 78: case 79: case 80: case 83: case 84:
   case 85: case 86: case 87: case 88: case 97: case 98: case 105: case 113:
   case 115: case 116: case 117: case 130: case 138: case 139: case 140:
   case 141: case 161: case 167: case 168: case 169: case 174: case 175:
   case 177: case 178: case 179: case 180: case 181: case 191:
   {
    cmd=-1; // noop
   } break;
  }
 }

 switch(cmd)
 {
  case 1 : // frecciagiu'
   if (crnext()) // se esiste la prossima riga
   {
    putelm((elem *)1,0,vidy); // deselez
    if (vidy==ya) scroll(yda+2,ya,yda+1); else vidy++;
    next(); rdw|=9;
   }
   else
   if (vidy==ya) // se ultima riga
   {
    scroll(yda+2,ya,yda+1); vidy--;
    vputstr(EOF_STR,EOF_STR_LEN,ya,config.col[3]); rdw|=8;
   }
  break;
  case 2 : // frecciasu'
   if (crprev()) // se esiste la riga precedente
   {
    putelm((elem *)1,0,vidy); // deselez
    if (vidy==yda+1) scroll(yda+1,ya-1,yda+2); else vidy--;
    prev(); rdw|=9;
   }
  break;
  case 3 : // pagina giu'
   if (crnext())
   {
    int y;

    y=ya-yda-1;
    while (crnext() && y>0) { next(); y--; }
    rdw|=12;
   }
  break;
  case 4 : // pagina su
   if (crprev()) // se esiste la riga precedente
   {
    int y; // row to scroll calcolati

    y=ya-yda-1;
    while (crprev() && y>0) { prev(); y--; }
    rdw|=12;
   }
  break;
  case 5 : // goto start of file
   if (crprev()) { corr=prima; lcorr=0; vidy=yda+1; rdw|=10; x=0; }
  break;
  case 6 : // goto end of file
   if (crnext() || (vidy!=ya-1 && lulti>=ya-yda-2))
   {
    corr=ulti; lcorr=lulti; vidy=ya-1; rdw|=12; x=rlung();

   }
  break;
  case 7 : // goto start of screen
   if (vidy>yda+1)
   {
    putelm((elem *)1,0,vidy);
    while (vidy>yda+1) { prev(); vidy--; }
    rdw|=9;
   }
  break;
  case 8 : // goto end of screen
   if (vidy<ya)
   {
    putelm((elem *)1,0,vidy);
    while (vidy<ya && crnext()) { next(); vidy++; }
    rdw|=9;
   }
  break;
  case 9 : if (this!=delbuf) savefile(); break; // savefile
  case 10 : ex=1;  break; // quit
  case 11 : ex=2;  break; // split window & ask file
//  case 12 : break; // escape
  case 13 : ex=17; break; // query & quit
  case 14 : ex=3;  break; // quit all
  case 15 : ex=18; break; // quit all con conferma
  case 16 : ex=19; break; // save all unsaved file
  case 17 : ex=7;  break; // window resize
  case 18 : ex=9;  break; // window next
  case 19 : ex=10; break; // window prev
  case 20 : ex=6;  break; // file load
  case 21 : ex=11; break; // next buffer
  case 22 : ex=12; break; // prev buffer
  case 23 : ex=5;  break; // file subst
  case 24 : ex=13; break; // zoom finestra
  case 25 : ex=14; break; // zoom finestra & query quit tutti gli altri edt
  case 26 : ex=20; break; // buffer selez.
  case 27 : // clip del SE non sono in clipboard
   if (clip.edtr!=this) { clip.edtr->reset(); winchk(clip.edtr); }
  break;
  case 30 : if (block.blk==7) rdw|=doblockclip(5); break; // clip cut
  case 31 : if (block.blk==7) rdw|=doblockclip(4); break; // clip copy
  case 32 : if (clip.edtr->rtot()) rdw|=doclipins(0); break; // clip paste
  case 33 : if (block.blk==7) rdw|=doblockclip(2); break; // blk del in delbuf
  case 34 : if (clip.edtr->rtot()) rdw|=doclipins(1); break; // clip paste & del
  case 35 : ex=22; break; // new filename
  case 36 : ex=23; break; // save clipboard
  case 37 : ex=24; break; // load clipboard
  case 38 : delbuf->reset(); break; // delete delete buffer
  case 39 : // restore dell'ultima linea del del buffer
   if (delbuf->rtot()>0)
   {
    delbuf->nseek(delbuf->rtot()-1);
    add(delbuf->rdati(),delbuf->rlung(),1); prev();
    delbuf->remove();
    rdw|=4;
    winchk(delbuf);
   }
  break;
  case 40 : // erase line
  {
   if (block.blk&4 && block.edtr==this && !block.type)
   {
    blockok();

    if (lcorr<block.start) { block.start--; block.end--; }
    else
    if (lcorr>=block.start && lcorr<=block.end)
    {
     if (block.start==block.end && block.end==lcorr) block.blk&=3;
     else if (block.start==lcorr) block.cstart=0;
     else if (block.end==lcorr) block.cend=1023;
     if (block.blk&4) block.end--;
    }
   }

   for (int i=0; i<4; i++) if (lcorr<posiz[i]) posiz[i]--;

   if (delbuf->add(rdati(),rlung(),3)) confirm(s_oom,0,yda,config.col[5]);
   rdw|=128;//okdelbuf();
   if (!crnext() && vidy>yda+1) { vidy--; rdw|=8; }
   if (docmode && crprev()) isetsoft(crprev(),softret());
   remove();

   colcheck(corr);

   rdw|=18; // cancello la riga
   if (rtot()==0) add("",0,3);
   winchk(delbuf);
   flag|=FL_RMODIF; setmodif(); rdw|=32;
  }
  break;
  case 41 : find(0); break; // ricerca interattiva
  case 42 : find(3); break; // ripete ricerca/sostituzione
  case 43 : find(1); break; // ricerca
  case 44 : find(2); break; // sostituzione
  case 47 : ex=28; break; // redraw all
  case 48 : ex=27; break; // compile config file
  case 49 : // goto start of block
   if ((block.blk&1) && block.edtr==this)
   {
    gotol(block.start); x=block.cstart;
    rdw|=44;
   }
  break;
  case 50 : // goto end of block
   if ((block.blk&2) && block.edtr==this)
   {
    gotol(block.end);
    x=block.cend; if (x==1023) x=0;
    rdw|=44;
   }
  break;
  case 51 : vidy=yda+1; rdw|=12; break; // current to start of screen
  case 52 : vidy=ya; rdw|=12; break; // current to end of screen
  case 53 : vidy=yda+(ya-yda)/2; rdw|=12; break; // current to middle of screen
  case 54 : // scrollup
  {
   if (crnext())
   {
    scroll(yda+2,ya,yda+1);
    next(); rdw|=13;
   }
  } break; // scrollup
  case 55 :
  {
   if (lcorr>vidy-yda-1 && crprev()) // se esiste la riga precedente
   {
    scroll(yda+1,ya-1,yda+2);
    prev(); rdw|=13;
   }
  } break; // scrolldown
  case 58 : // goto line n.
  {
   dialog number;
   char str[80];

   str[0]='\0';
   if (number.ask("Goto line ?",yda,str)==0) { gotol(atol(str)-1); rdw|=44|512; }
  }
  break;
  case 60 : // start/rec macro
   if (macrocmd) recmacro(); else startmacro(yda); rdw|=32;
  break;
  case 61 : // start macro
   if (macrocmd) abortmacro(); startmacro(yda); rdw|=32;
  break;
  case 62 : abortmacro(); rdw|=32; break; // abortmacro
  case 63 : if (macrocmd) recmacro(); rdw|=32; break;
  case 64 : posiz[0]=lcorr; posizx[0]=x; break; // set segnalino 0
  case 65 : posiz[1]=lcorr; posizx[1]=x; break; // set segnalino 1
  case 66 : posiz[2]=lcorr; posizx[2]=x; break; // set segnalino 2
  case 67 : posiz[3]=lcorr; posizx[3]=x; break; // set segnalino 3
  case 68 : gotol(posiz[0]); x=posizx[0]; rdw|=44|512; break; // goto segnalino 0
  case 69 : gotol(posiz[1]); x=posizx[1]; rdw|=44|512; break; // goto segnalino 1
  case 70 : gotol(posiz[2]); x=posizx[2]; rdw|=44|512; break; // goto segnalino 2
  case 71 : gotol(posiz[3]); x=posizx[3]; rdw|=44|512; break; // goto segnalino 3
  case 72 : // set right margin
  {
   dialog number;
   char str[80];

   sprintf(str,"%i",config.rmargin);
   number.ask("Set right margin to:",yda,str);
   config.rmargin=atoi(str);
  }
  break;
  case 73 : // set left margin
  {
   dialog number;
   char str[80];

   sprintf(str,"%i",config.lmargin);
   number.ask("Set left margin to:",yda,str);
   config.lmargin=atoi(str);
  }
  break;
  case 74 : // set paragraph indent margin
  {
   dialog number;
   char str[80];

   sprintf(str,"%i",config.rient);
   number.ask("Set paragraph indent to:",yda,str);
   config.rient=atoi(str);
  }
  break;
  case 75 : // rende il return un soft
   if (docmode) { setsoft(1); rdw|=512; }
  break;
  case 76 : // formatta il paragrafo
  if (docmode && pari!=-1 && pare!=-1)
  {
   int movepos=lcorr,savepos=lcorr,riga;
   int fix=0;
   int newx=x;
   int rm=config.rmargin-(mailquote?strlen(mailquote):0);

   for (riga=pari; riga<=pare; riga++)
   {
    nseek(riga);
    started();     // prendo la riga

    int t=config.lmargin; if (riga==pari) t+=config.rient;


    if (t+lbuf<1024) // aggiusto il margine sinistro
    {
     memmove(buffer+t,buffer,lbuf); memset(buffer,' ',t); lbuf+=t;
     if (movepos==lcorr) newx+=t;
    }

    while ((lbuf<rm && pare>riga) && irlung(crnext())+lbuf<1023)
    {
     int oldl=lbuf;
     int ll=irlung(crnext());
     if (ll)
     {
      oldl++;
      buffer[lbuf++]=' '; // aggiungo lo spazio in fondo
      memcpy(buffer+lbuf,irdati(crnext()),ll);
      lbuf+=ll;
     }
     if ((movepos==lcorr+1) && (newx+oldl<1024)) newx+=oldl;
     if (movepos>=lcorr+1) movepos--;

     next();
     int l=lcorr;
     remove(); fix--;
     if (lcorr==l) prev();
     pare--;
    }

    int tt=rm;
    if (lbuf>tt)
    {
     while(tt>0 && buffer[tt-1]!=' ') tt--; // cerco il primo spazio
     if (tt>t)
     {
      soft=1;
      add(buffer+tt,(lbuf-tt)|0x4000,2); pare++; lbuf=tt; // 1==lo spazio
      fix++;

      if (movepos==lcorr && newx>=tt)
      {
       if ((newx-(lbuf))>-1) newx-=(lbuf);
       movepos++;
      } else
      if (movepos>lcorr) movepos++;
     }
    }

    if (riga==pare) soft=0;
    flag|=FL_RMODIF;
    update();
   }
   if (savepos>rtot()) savepos=rtot()-1;
   nseek(savepos); x=newx;
   rdw|=514|8;
   if ((block.blk==7) && block.edtr==this)
   {
    if (block.start>lcorr) block.start+=fix;
    if (block.end>lcorr) block.end+=fix;
   }
   internal=1;
   while (movepos<savepos) { execcmd(2); movepos++; }
   while (movepos>savepos) { execcmd(1); movepos--; }
   internal=0;
  }
  break;
  case 77 : // giustifica
  if (docmode && pari!=-1 && pare!=-1)
  {
   int savepos=lcorr,riga;
   int rm=config.rmargin-(mailquote?strlen(mailquote):0);

   for (riga=pari; riga<pare; riga++)
   {
    nseek(riga);
    started();

    int toadd;
    toadd=rm-lbuf;

    while (toadd>0)
    {
     int t,ota,p;
     ota=toadd;
     for (t=0; t<lbuf && buffer[t]==' '; t++);

     for (p=lbuf-1; p>t && toadd>0; p--)
     {
      if (buffer[p]==' ')
      {
       if (savepos==lcorr && x>p) x++;
       memmove(buffer+p+1,buffer+p,lbuf-p); buffer[p]=' '; toadd--; lbuf++;
       while(p>t && buffer[p-1]==' ') p--;
      }
     }
     flag|=FL_RMODIF;
     if (toadd==ota) break;
    }
    update();
   }
   nseek(savepos);

   rdw|=512|8;
  }
  break;
  case 78 : // degiustifica
  if (docmode && pari!=-1 && pare!=-1)
  {
   int savepos=lcorr,riga;

   for (riga=pari; riga<=pare; riga++)
   {
    nseek(riga);
    started();

    int t;
    for (t=0; t<lbuf && buffer[t]==' '; t++) if (x>0 && savepos==lcorr) x--;
    lbuf-=t; memmove(buffer,buffer+t,lbuf);

    char *p,*ps;
    int lb,pos;
    p=buffer; lb=lbuf;
    while ((ps=(char *)memchr((void *)p,' ',lb))!=0)
    {
     pos=(int)(ps-buffer);
     if (*(ps+1)==' ' && pos+1<=lbuf)
     {
      memmove(ps,ps+1,lb); lb--; lbuf--;
      if (savepos==lcorr && pos<x) x--;
     }
     else { lb-=(int)(ps-p)+1; p=ps+1; }
    }

    flag|=FL_RMODIF;
    update();
   }
   nseek(savepos);
   rdw|=512|8;
  }
  break;
  case 79 : // rende il return un hard
   if (docmode) { setsoft(0); rdw|=512; }
  break;
  case 80 : // rende tutti gli enter dei hard enter
  {
   int l;
   elem *scorr=prima;

   for (l=0; l<rtot(); l++)
   {
    isetsoft(scorr,0);
    scorr=irnext(scorr);
   }
  }
  break;
  case 81 : writemacro(yda); break; // write macro
  case 82 : // goto parentesi relativa
  {
   char ch,fw,bw;
   int nx=0;

   if (rlung()>x) ch=*(rdati()+x); else ch=' ';

   fw=bw=0;

   switch(ch)
   {
    case '(' : fw='('; bw=')'; nx=1; break; //)
    case '<' : fw='<'; bw='>'; nx=1; break;
    case '[' : fw='['; bw=']'; nx=1; break; //]
    case '{' : fw='{'; bw='}'; nx=1; break; //}((
    case ')' : fw=')'; bw='('; nx=0; break; //)
    case '>' : fw='>'; bw='<'; nx=0; break; //[[
    case ']' : fw=']'; bw='['; nx=0; break; //{{]
    case '}' : fw='}'; bw='{'; nx=0; break; //}
   }

   if (fw)
   {
    int gtl;
    char *p;
    int l,pos,cnt;
    elem *scorr;

    if (nx)
    {
     p=rdati()+x; l=rlung()-x;
     scorr=corr; gtl=lcorr; pos=x; cnt=0;
     if (!nx) l=x;
     while(scorr)
     {
      int i;

      for (i=l; i>0; i--)
      {
       ch=*p;
       if (ch==fw) cnt++;
       if (ch==bw) { cnt--; if (cnt==0) break; }
       p++; pos++;
      }
      if (cnt==0) break;
      scorr=irnext(scorr); gtl++;
      if (scorr) { l=irlung(scorr); p=irdati(scorr); pos=0; }
     }
    }
    else
    {
     p=rdati()+x;
     l=x+1;
     scorr=corr; gtl=lcorr; pos=x; cnt=0;
     while(scorr)
     {
      int i;

      for (i=l; i>0; i--)
      {
       ch=*p;
       if (ch==fw) cnt++;
       if (ch==bw) { cnt--; if (cnt==0) break; }
       p--; pos--;
      }
      if (cnt==0) break;
      scorr=irprev(scorr); gtl--;
      if (scorr) { l=irlung(scorr); p=irdati(scorr)+l-1; pos=l-1; }
     }
    }
    if (cnt==0 && scorr) { x=pos; gotol(gtl); rdw|=44|512; }
   }
  }
  break;
  case 83 : // fill block
  if (block.blk==7 && block.edtr==this)
  {
   blockok();
   dialog fill;
   char str[80];
   str[0]='\0';
   if (fill.ask("Fill character(s) ?",yda,str)) break;
   int pos=0,len=strlen(str); if (len==0) break;

   int savepos=lcorr,riga;

   for (riga=block.start; riga<=block.end; riga++)
   {
    nseek(riga);
    started();

    int st,ed;

    st=0; ed=1023;
    if (block.type || riga==block.start) st=block.cstart;
    if (block.type || riga==block.end) ed=block.cend;

    if (ed>lbuf && !block.type) ed=lbuf;

    int i;
    for (i=st; i<ed; i++) { buffer[i]=str[pos++]; if (pos==len) pos=0; }
    lbuf=lbuf>?block.cend;
    flag|=FL_RMODIF;
    update();
   }
   nseek(savepos);
   rdda=block.start; rda=block.end;
   rdw|=256;
  }
  break;
  case 84 : // block to upper
  case 85 : // block to lower
  case 86 : // block flip case
  if (block.blk==7 && block.edtr==this)
  {
   blockok();
   int savepos=lcorr,riga;

   for (riga=block.start; riga<=block.end; riga++)
   {
    nseek(riga);
    started();

    int st,ed;

    st=0; ed=1023;
    if (block.type || riga==block.start) st=block.cstart;
    if (block.type || riga==block.end) ed=block.cend;

    if (ed>lbuf) ed=lbuf;

    int i;
    for (i=st; i<ed; i++)
    {
     switch(cmd)
     {
      case 84 : buffer[i]=toupper(buffer[i]); break;
      case 85 : buffer[i]=tolower(buffer[i]); break;
      case 86 :
       if (isupper(buffer[i])) buffer[i]=tolower(buffer[i]);
                          else buffer[i]=toupper(buffer[i]);
      break;
     }
    }
    flag|=FL_RMODIF;
    update();
   }
   nseek(savepos);
   rdda=block.start; rda=block.end;
   rdw|=256;
  }
  break;
  case 87 : // block left
  case 88 : // block right
  if (block.blk==7 && block.edtr==this)
  {
   blockok();
   int savepos=lcorr,riga;

   for (riga=block.start; riga<=block.end; riga++)
   {
    nseek(riga);
    started(); if (lbuf>1022) lbuf=1022;

    if (cmd==88) { memmove(buffer+1,buffer,lbuf++); buffer[0]=' '; }
    else
//    if (lbuf && buffer[0]==' ') // Michele's version (???)
    if (lbuf)
    { memmove(buffer,buffer+1,--lbuf); buffer[lbuf]=' '; }
    flag|=FL_RMODIF;
    update();
   }
   nseek(savepos);
   rdda=block.start; rda=block.end;
   rdw|=256;
  }
  break;
  case 89 : // goto next paragraph
  {
   elem *scorr=corr;
   int nn=lcorr;

   while (scorr && isoftret(scorr)) { scorr=irnext(scorr); nn++; }
   nn++;
   if (scorr && nn<rtot()) { gotol(nn); rdw|=44|512; }
  }
  break;
  case 90 : // goto previus paragraph
  {
   elem *scorr=corr;
   int nn=lcorr;

   if (nn>1)
   {
    nn--; scorr=irprev(scorr);
    while (irprev(scorr) && isoftret(irprev(scorr)))
    { scorr=irprev(scorr); nn--; }
    if (scorr) { gotol(nn); rdw|=44|512; }
   }
  }
  break;
  // case 91 non lo posso usare, openall !
  case 92 : ex=33; break; // file reopen
  case 93 : ex=34; break; // file reopen no query
  case 94 : clip.pastefrom=clip.edtr->rtot(); break; // clip paste from here
  case 95 : clip.pastefrom=0L; break; // clip paste from zero
  case 96 : break; // dummy command
  case 97 : // internal block left
  case 98 : // internal block right
  if (block.blk==7 && block.edtr==this && block.cstart<block.cend
   && block.type)
  {
   blockok();
   int savepos=lcorr,riga;

   for (riga=block.start; riga<=block.end; riga++)
   {
    nseek(riga);
    started(); if (lbuf>1023) lbuf=1022;

    if (cmd==98)
    {
 memmove(buffer+1+block.cstart,buffer+block.cstart,block.cend-block.cstart-1);
     buffer[block.cstart]=' ';
     if (lbuf<block.cend && lbuf>block.cstart) lbuf++;
    }
    else if (lbuf)
    {
 memmove(buffer+block.cstart,buffer+1+block.cstart,block.cend-block.cstart-1);
     buffer[block.cend-1]=' ';
     if (lbuf<block.cend && lbuf>block.cstart) lbuf--;
    }
    flag|=FL_RMODIF;
    update();
   }
   nseek(savepos);
   rdda=block.start; rda=block.end;
   rdw|=256;
  }
  break;
  case 99 : // C functions search
  {
// nello scorrere il file, tolgo commenti. le stringhe e le costanti carattere
// le tolgo solo se iniziano e finiscono nella stessa riga. else troppa fatica.
   char *sf=(colorize==6?(char *)"Java functions":(char *)"C functions");

   selez cfun(sf);
   int num=0,len,graffe=0,cc=0,commento=0;
   elem *scorr=prima;
   char *p;

   for (int l=0; l<rtot(); l++,scorr=irnext(scorr))
   {
    char f=0,str[80];

    len=irlung(scorr);
    if (len==0) continue;
    memcpy(buffer,irdati(scorr),len);
    buffer[len]='\0';

    if ((p=strstr(buffer,"//"))!=0) *p='\0'; // tolgo i rem nella riga

    if (commento)
    {
     char *p1;
     if ((p1=strstr(buffer,"*/"))!=0) { strcpy(buffer,p1+2); commento=0; }
     else *buffer='\0';
    }

    while ((p=strstr(buffer,"/*"))!=0)
    {
     *p='X';
     char *p1;
     if ((p1=strstr(p+1,"*/"))!=0) strcpy(p,p1+2);
     else { commento=1; *p='\0'; }
    }
    while ((p=strchr(buffer,'"'))!=0)
    {
     *p='X';
     char *p1; if ((p1=strchr(p+1,'"'))!=0) strcpy(p,p1+1);
    }
    while ((p=strchr(buffer,'\''))!=0)
    {
     *p='X';
     char *p1; if ((p1=strchr(p+1,'\''))!=0) strcpy(p,p1+1);
    }

    while(isspace(buffer[0])) strcpy(buffer,buffer+1);
    len=strlen(buffer); if (len==0) continue;
    p=buffer+strlen(buffer)-1;
    while(isspace(*p) && p>=buffer) { *p='\0'; p--; }
    len=strlen(buffer); if (len==0) continue;
    // adesso ho la stringa filtrata

    while((p=strchr(buffer,'{'))!=0) { *p=' '; graffe++; f=1; cc=1; }
    while((p=strchr(buffer,'}'))!=0)
    { *p=' '; graffe--; f=1; if (graffe<0) graffe=0; }

    if (f) continue;

    if (graffe>(colorize==6?1:0)) continue; // se ci sono graffe, non e' una dichiarazione

    // controllo che ci sia almeno un "(" e //un ")"
    if (strchr(buffer,'(')==0) continue;
//    if (strchr(buffer,')')==0) continue;
    // controllo che non ci sia un ";" (dichiarazione)
    if (strchr(buffer,';')!=0) continue;
    // controllo che non cominci per "#" (preprocessore)
    if (*buffer=='#') continue;

    buffer[60]='\0'; // trunc
    sprintf(str,"%5i:%s",(int)l+1,buffer);
    if (cfun.add(str,strlen(str)+1,cfun.sseek(str))) l=rtot();
    num++;

//    scorr=irnext(scorr);
   }
   if (num>0 && cc) // se esiste qualche funzione e qualche graffa
   {
    char str[80];

    if (cfun.goselez(str,yda,0)==0) // 0:enter, 1:esc
    {
     gotol(atol(str)-1); rdw|=44|512;
    }
    rdw|=8;
   }
   else confirm("No C functions found. press ESC ",0,yda,config.col[6]);
  }
  break;
  // il 100 e' impegnato come e_entwait
  case 101 : ex=35; break; // save window
  case 102 : ex=36; break; // goto window
  case 103 : ex=37; break; // save only modified buffer
  case 104 : // init (gotoline eventuale)
  {
   if (config.gotoline!=-1)
   { gotol(config.gotoline-1); rdw|=44|512; config.gotoline=-1; }
  }
  break;
  case 105 : // indent block
  if (block.blk==7 && block.edtr==this)
  {
   int indf00,ind0f0,ind00f;
   int parent=0;
   int indent=0;
   int incomm=0,instr=0;
   int i;
   char row[1030];

   indf00=(config.indcfg&0xf00)>>8; //if (indf00==0) indf00=1;
   ind0f0=(config.indcfg&0x0f0)>>4; //if (ind0f0==0) ind0f0=1;
   ind00f=(config.indcfg&0x00f);    //if (ind00f==0) ind00f=1;

   blockok();
   int savepos=lcorr,riga;

   for (riga=block.start; riga<=block.end; riga++)
   {
    nseek(riga);
    started(); if (lbuf>1022) lbuf=1022;
    memcpy(row,buffer,lbuf); // copio nel buffer

    for (i=0; i<lbuf; i++) if (row[i]=='\t') row[i]=' ';

    // eliminazione stringhe
    for (i=0; i<lbuf; i++)
    {
     if (row[i]=='\"' && instr) { row[i]='@'; instr=0; }
     else
     if (row[i]=='\"' && !instr) instr=1;
     if (instr) row[i]='@';
    }

    // eliminazione commento singolo
    {
     char *p;
     if ((p=strstr(row,"//"))) for (i=(p-row); i<lbuf; i++) row[i]='@';
    }

    // eliminazione commento doppio
    for (i=0; i<lbuf-1; i++)
    {
     if (row[i]=='*' && row[i+1]=='/' && incomm)
     { row[i]='@'; row[i+1]='@'; incomm=0; i++; }
     else
     if (row[i]=='/' && row[i+1]=='*' && !incomm)
     { row[i]='@'; i++; incomm=1; }
     if (incomm) row[i]='@';
    }

    // sistemo il primo indent
    if (riga==block.start)
    {
     indent=0;
     for (i=0; (row[i]==' ' && i<lbuf); i++) indent++;
    }

    // tolgo tutti gli spazi prima e dopo le graffe
    for (i=0; i<lbuf-1; i++)
    {
     if (row[i]==' ' && (row[i+1]=='{' || row[i+1]=='}'))
     {
      int j;

      for (j=i; row[j]==' ' && j>0; j--);

      if (row[j]!=' ') j++;
      memcpy(row+j,row+i+1,lbuf-(i-j+1));
      memcpy(buffer+j,buffer+i+1,lbuf-(i-j+1));
      lbuf-=i-j+1;
      i=j;
     }
     if ((row[i]=='{' || row[i]=='}') && row[i+1]==' ' && lbuf>i+1)
     {
      i++;
      int j;

      for (j=i; row[j]==' ' && j<lbuf; j++);
      if (row[j]!=' ') j--;

      memcpy(row+i,row+j+1,lbuf-(j-i+1));
      memcpy(buffer+i,buffer+j+1,lbuf-(j-i+1));
      lbuf-=j-i+1;
      i--;
     }
    }

    // aggiungo gli spazi "*{"
    for (i=0; i<lbuf; i++)
    {
     if (row[i]=='{' && lbuf+ind0f0<1020) //}
     {
      memmove(row+ind0f0+i,row+i,lbuf-i);
      memmove(buffer+ind0f0+i,buffer+i,lbuf-i);
      memset(row+i,' ',ind0f0);
      memset(buffer+i,' ',ind0f0);
      lbuf+=ind0f0;
      i+=ind0f0;
     }
    }

    // aggiungo gli spazi "{*"
    for (i=0; i<lbuf-1; i++)
    {
     if (row[i]=='{' && lbuf+indf00<1020) //}
     {
      memmove(row+indf00+1+i,row+1+i,lbuf-(i+1));
      memmove(buffer+indf00+1+i,buffer+1+i,lbuf-(i+1));
      memset(row+1+i,' ',indf00);
      memset(buffer+1+i,' ',indf00);
      lbuf+=indf00;
      i+=indf00;
     }
    }

    // aggiungo gli spazi "*}"
    for (i=0; i<lbuf; i++)
    {
     if (row[i]=='}' && lbuf+indf00<1020) //{
     {
      memmove(row+indf00+i,row+i,lbuf-i);
      memmove(buffer+indf00+i,buffer+i,lbuf-i);
      memset(row+i,' ',indf00);
      memset(buffer+i,' ',indf00);
      lbuf+=indf00;
      i+=indf00;
     }
    }

    // aggiungo gli spazi "}*"
    for (i=0; i<lbuf-1; i++)
    {
     if (row[i]=='}' && lbuf+ind0f0<1020) //{
     {
      memmove(row+ind0f0+1+i,row+1+i,lbuf-(i+1));
      memmove(buffer+ind0f0+1+i,buffer+1+i,lbuf-(i+1));
      memset(row+1+i,' ',ind0f0);
      memset(buffer+1+i,' ',ind0f0);
      lbuf+=ind0f0;
      i+=ind0f0;
     }
    }

    // tolgo gli spazi in testa
    for (i=0; (row[i]==' ' && i<lbuf); i++);
    lbuf-=i;
    memcpy(row,row+i,lbuf);
    memcpy(buffer,buffer+i,lbuf);

    // conto le parentesi
    parent=0;
    for (i=0; i<lbuf; i++)
    {
     if (row[i]=='{') parent++;
     else
     if (row[i]=='}') parent--;
    }

    // spazio l'inizio della riga
    if (riga!=block.start && parent<0) indent+=parent*ind00f;
    indent=(indent>?0);
    if (lbuf+indent<1020)
    {
     memmove(row+indent,row,lbuf);
     memmove(buffer+indent,buffer,lbuf);
     memset(row,' ',indent);
     memset(buffer,' ',indent);
     lbuf+=indent;
    }
    if (parent>0) indent+=parent*ind00f;

    flag|=FL_RMODIF;
    update();
   }
   nseek(savepos);
   rdda=block.start; rda=block.end;
   rdw|=256;
  }
  break;
  #ifdef X11
   case 106: exportclip(0,1); break; // export clip
   case 107: exportclip(1,1); break; // export clip with del
   case 108: exportclip(0,0); break; // export clip
   case 109: exportclip(1,0); break; // export clip with del
   case 110:
   {
    int retry;
    Atom prop;

    prop=XInternAtom(display,"ZED_SELECTION",False);
    XConvertSelection(display,XA_PRIMARY,XA_STRING,prop,zed_win,
                      sele.time);

    for (retry=0; retry<100; retry++) if (getxkey()==0xfffffffc) retry=100;
   }
   break; // request selection; import clip
  #endif
  case 111 : askfile(0,0,yda,ya,(editor *)1); break; // add config
  case 112 : ex=38;  break; // split window & ask file OR load file in window
  case 113 : // eliminazione run-time di tutti i tab del file
  {
   elem *scorr=prima;
   char row[1030];
   int savepos=lcorr;
   int l,l2,fl,mod;

   do
   {
    l2=l=irlung(scorr)&0x03ff;
    fl=irlung(scorr)&0xfc00;

    memcpy(row,irdati(scorr),l); // copio nel buffer

    short int l3=(short int)l2;
    mod=tabexp(row,&l3);
    l2=l3;

    if (mod)
    {
     flag|=FL_MODIF;
     isubst(&scorr,row,l2|fl);
    }

    scorr=irnext(scorr);
   } while(scorr);

   nseek(savepos);
   rdw|=4|32;
  } break;
  // case 114 non lo posso usare, e' openbinary !
  case 115 : // crea paragrafo con le righe selezionate dal blocco
  {
   if (block.blk)
   {
    blockok();

    elem *scorr;
    int slcorr,i;

    inseek(&scorr,slcorr,block.start);

    if (irprev(scorr)) isetsoft(irprev(scorr),0);

    for (i=0; i<block.end-block.start; i++)
    {
     isetsoft(scorr,1);
     scorr=irnext(scorr);
    }
    isetsoft(scorr,0);
   }
  }
  break;
  case 116: // toglie i quote dal paragrafo corrente
  {
   if (colorize==5) // modo MAIL???
   {
    // rilevazione parte in comune!

    char common[1024];

    elem *scorr;
    int slcorr,i,riga;

    inseek(&scorr,slcorr,pari);

    memcpy(common,irdati(scorr),irlung(scorr));

    common[irlung(scorr)]='\0';

    for (riga=pari+1; riga<=pare; riga++)
    {
     scorr=irnext(scorr);
     char *p=irdati(scorr);

     for (i=0; common[i]==p[i] && common[i]!='\0'; i++); common[i]='\0';
    }

    // il quote finisce per '>'

    i=strlen(common);

    for (i=strlen(common); i>=0 && common[i]!='>'; i--) common[i]='\0';

    if (mailquote) { delete mailquote; mailquote=0; }

    i=strlen(common);

    if (i)
    {
     mailquote=new char[strlen(common)+1];
     strcpy(mailquote,common);

     // adesso TOLGO il quote dalle righe

     int savepos=lcorr;

     for (riga=pari; riga<=pare; riga++)
     {
      nseek(riga);
      started();

      flag|=FL_RMODIF;
      lbuf-=i; if (lbuf<0) lbuf=0;

      memmove(buffer,buffer+i,lbuf);

      update();
     }
     nseek(savepos);
     x-=i; if (x<0) { mailpos=x+i; x=0; } else mailpos=-1;
    }
   }
   rdw|=512|8;
  } break;

  case 117: // rimette i quote alle righe del paragrafo
  {
   int riga;
   int i=mailquote?strlen(mailquote):0;

   if (i)
   {
    // adesso RIMETTO il quote dalle righe

    int savepos=lcorr;

    for (riga=pari; riga<=pare; riga++)
    {
     nseek(riga);
     started();

     flag|=FL_RMODIF;

     memmove(buffer+i,buffer,lbuf);
     memcpy(buffer,mailquote,i);

     lbuf+=i; if (lbuf>1023) lbuf=1023;

     update();
    }
    nseek(savepos);
    x+=i;

    if (mailpos>-1) { x=mailpos; mailpos=-1; }

    delete mailquote; mailquote=0;
   }
   rdw|=512|8;
  } break;
  // case 118 non lo posso usare, e' open-readonly !

  case 119: // rot13
  if (block.blk==7 && block.edtr==this)
  {
   blockok();
   int savepos=lcorr,riga;

   for (riga=block.start; riga<=block.end; riga++)
   {
    nseek(riga);
    started();

    int st,ed;

    st=0; ed=1023;
    if (block.type || riga==block.start) st=block.cstart;
    if (block.type || riga==block.end) ed=block.cend;

    if (ed>lbuf) ed=lbuf;

    int i;
    for (i=st; i<ed; i++)
    {
     char *ch=&buffer[i];
     if (isupper(*ch))
     {
      (*ch)+=13;
      if ((*ch)>'Z') (*ch)-=26;
     }
     else
     if (islower(*ch))
     {
      (*(unsigned char *)ch)+=13;
      if ((*(unsigned char *)ch)>'z') (*(unsigned char *)ch)-=26;
     }
    }
    flag|=FL_RMODIF;
    update();
   }
   nseek(savepos);
   rdda=block.start; rda=block.end;
   rdw|=256;
  }
  break;

/************************************/
/************************************/

  case 130 : // Canc - delete
   if (config.nospace && this!=clip.edtr)
   {
    while (lbuf>0 && (buffer[lbuf-1]==' ' || buffer[lbuf-1]==9)) lbuf--;
   }
   if (x<lbuf)
   {
    memmove(buffer+x,buffer+x+1,lbuf-x); lbuf--; flag|=FL_RMODIF; rdw|=1;
    if (block.blk&4 && block.edtr==this && !block.type)
    {
     blockok();
     if (block.start==lcorr && x<block.cstart) block.cstart--;
     if (block.end==lcorr && x<block.cend) blkcend(-1);
    }
   }
   else if (x>=lbuf && crnext())
   { // devo riunire due righe
    elem *p;

    if (block.blk&4 && block.edtr==this && !block.type)
    { // aggiusto il blocco
     blockok();
     if (block.start==lcorr+1) { block.start--; block.cstart+=x; }
     if (block.end==lcorr+1) { block.end--; blkcend(x); }

     if (lcorr+1<block.start) { block.start--; block.end--; }
     else
     if (lcorr+1>block.start && lcorr+1<=block.end) block.end--;
    }

    for (int i=0; i<4; i++) if (lcorr<posiz[i]) posiz[i]--;
    if (x>lbuf) lbuf=x;
    int ll=((MAXLINE-x)<?(irlung(crnext())))>?0;
    memcpy(buffer+x,irdati(crnext()),ll);
    soft=isoftret(crnext());
    lbuf+=ll;
    p=corr; next(); remove();
    if (p!=corr) prev();
    rdw|=18|32; setmodif();
   }
   flag|=FL_RMODIF;
  break;
  case 131 : if (x<MAXLINE-2) { x++; rdw|=8; } break; // freccia destra
  case 132 : // freccia sinistra
   if (x>0) { x--; rdw|=8; }
   else { execcmd(2); execcmd(133); }
   break;
  case 133 : // go to end of line
   for (x=lbuf; buffer[x]==' ' && x>0; x--);
   if (lbuf>0 && x<(MAXLINE-1)) x++;
   if (x==MAXLINE) x--;
   rdw|=8;
  break;
  case 134 : if (x>0) { x=0; rdw|=8; } break; // go to start of line
  case 135 : // Backspace
   if (config.nospace && this!=clip.edtr)
   {
    while (lbuf>0 && (buffer[lbuf-1]==' ' || buffer[lbuf-1]==9)) lbuf--;
   }
   if (x<=lbuf || (lbuf==0 && x==0))
   {
    if (x>0)
    {
     if (block.blk&4 && block.edtr==this && !block.type)
     {
      blockok();
      if (block.start==lcorr && x<=block.cstart) block.cstart--;
      if (block.end==lcorr && x<=block.cend) blkcend(-1);
     }
     if (cmd==135 && (!(flag&FL_SOVR))) // se backspace e inserimento
     { memmove(buffer+x-1,buffer+x,lbuf-x+1); lbuf--; }
     else buffer[x-1]=' ';
     flag|=FL_RMODIF; x--; rdw|=41;
    }
    else if (crprev()) // freccia in su, goto EOL, canc
    { execcmd(2); execcmd(133); execcmd(130); rdw|=32; }
   }
   else execcmd(132); // freccia sinistra
   flag|=FL_RMODIF;
  break;
  case 176 : // soft enter
  case 136 : // hard enter
  {
   int xx,xr;

   xr=x;
   if (config.indent) // calcolo xx (posizione primo carattere != da ' '
   {
    for (xx=0; xx<lbuf && buffer[xx]==' '; xx++);
    if (buffer[xx]==' ' || buffer[x]=='\t') xx=0;
   } else xx=0;
   if (xx==lbuf) xx=0;

   if (flag & FL_SOVR)
   { // se sovrascrittura
    internal=1; rdw|=execcmd(1); internal=0; /* giu */
    vx=x=xx; rdw|=8; /* goto SOL */
   }
   else
   {
    // x    : posizione del cursore
    // xr   : posizione del cursore (copia)
    // xx   : posizione del primo carattere !=' ' o -1 se non richiesto

    int divx,addc,newx;

    // preparo delle variabili
    // divx : 0 se devo dividere in due la riga in x
    //        1 se devo aggiungere una riga vuota dopo la corrente
    // addc : spazi da aggiungere alla riga nuova
    // newx : nuova x del cursore

    if (x==xx && x==0)                     { divx=0; addc=0;  newx=0; }
    else
    if (config.indent && x<xx)             { divx=0; addc=0;  newx=0; }
    else
    if (config.indent && x>=xx && x<=lbuf) { divx=0; addc=xx; newx=xx; }
    else
    if (config.indent && x>lbuf)           { divx=1; addc=0;  newx=xx; }
    else
    if (x<=lbuf)                           { divx=0; addc=0;  newx=0; }
    else                                   { divx=1; addc=0;  newx=0; }

    if (block.blk&4 && block.edtr==this && !block.type)
    { // se c'e' blocco e editor giusto e blocco a righe
     blockok();

     if (lcorr<block.start) { block.start++; block.end++; }
     else
     if (lcorr==block.start)
     { // sono sulla linea di partenza del blocco
      if (x<block.cstart)
      {
       if (lcorr==block.end) blkcend(-x+addc);
       block.start++; block.end++;
       block.cstart+=-x+addc;
      }
      else
      {
       if (block.end==lcorr)
       {
        if (block.cend>x) { blkcend(-x+addc); block.end++; }
       } else block.end++;
      }
     }
     else if (lcorr<block.end) block.end++;
     else
     if (lcorr==block.end)
      if (x<block.cend) { block.end++; blkcend(-x+addc); }
    }

    for (int i=0; i<4; i++) if (lcorr<posiz[i]) posiz[i]++;

    if (block.blk&4 && block.edtr==this && block.type)
    { // se c'e' blocco e editor giusto e blocco a colonne
     blockok();
     if (lcorr<block.start) { block.start++; block.end++; }
    }

    flag|=FL_RMODIF; setmodif();
    rdda=lcorr-1;
    if (!divx)
    { // divido la riga in due in x
     if (add(buffer+x,(lbuf-x)|soft,2)) confirm(s_oom,0,yda,config.col[5]);
     if (docmode) if (cmd==176) soft=0x4000; else soft=0;
     memset(buffer+x,' ',lbuf-x);
     lbuf=x;
    }
    else
    { // aggiungo una riga dopo
     if (docmode) if (cmd==176) soft=0x4000; else soft=0;
     if (add("",0,2)) confirm(s_oom,0,yda,config.col[5]);
    }
    internal=1; rdw|=execcmd(1); internal=0;
    rda=lcorr+ya-vidy;
    rdw|=256+32+16+8;

    if (addc)
    {
     started();
     if (addc+lbuf>1023) addc=1023-lbuf;
     memmove(buffer+addc,buffer,lbuf); memset(buffer,' ',addc);
     lbuf+=addc;
    }
    x=vx=newx;
   } // fine se inserimento
  }
  break;
  case 138 : // toggle insert/overwrite
   if (flag&FL_SOVR) flag=flag&(FL_SOVR^0xffff); else flag|=FL_SOVR;
   rdw|=32;
  break;
  case 139 : // set insert
   flag=flag&(FL_SOVR^0xffff); rdw|=32;
  break;
  case 140 : // putcode
  {
   unsigned int t;
   do
   #ifdef X11
   {
   #else
   {
   #endif
    t=gettst();
    #ifdef X11
    if (t!=XK_Return)
    #else
    if (t!=13)
    #endif
    {
     if (t>32 && t<126)
     {
      if (t=='\\' || t==';' || t=='&') execch('\\');
      execch(t);
     }
     else
     {
      execch('\\');
      if (t<256)
      {
       execch('u');
       execch(itoh((t&0x0000000f0)>>4));
       execch(itoh((t&0x00000000f)>>0));
      }
      else if (t<65536)
      {
       execch('w');
       execch(itoh((t&0x00000f000)>>12));
       execch(itoh((t&0x000000f00)>>8));
       execch(itoh((t&0x0000000f0)>>4));
       execch(itoh((t&0x00000000f)>>0));
      }
      else
      {
       execch(itoh((t&0x0f0000000)>>28));
       execch(itoh((t&0x00f000000)>>24));
       execch(itoh((t&0x000f00000)>>20));
       execch(itoh((t&0x0000f0000)>>16));
       execch(itoh((t&0x00000f000)>>12));
       execch(itoh((t&0x000000f00)>>8));
       execch(itoh((t&0x0000000f0)>>4));
       execch(itoh((t&0x00000000f)>>0));
      }
//      execch(itoh((t&0x0f0)>>4)); execch(itoh(t&0xf));
     }
    }
   #ifdef X11
   } while (t!=XK_Return && mouse.exec==-1);
   #else
   } while (t!=13);
   #endif
  }
  break;
  case 141 : // inscode
  {
   int t;
   do
   {
    t=gettst()&0xff; if (t!=13) execch(t&255);
   } while (t!=13);
  }
  break;
  case 142 : // go to next word
   if (x<lbuf)
   {
    while (isword(buffer[x]) && x<lbuf) x++;
    while (!isword(buffer[x]) && x<lbuf) x++;
    rdw|=8;
   } else { execcmd(1); execcmd(134); }
  break;
  case 143 : // go to previus word
   if (x>0)
   {
    x--;
    while (!isword(buffer[x]) && x>0) x--;
    if (x>0)
    {
     while (isword(buffer[x]) && x>0) x--;
     if (x>0 || !isword(buffer[0])) x++;
    }
    rdw|=8;
   } else { execcmd(2); execcmd(133); }
  break;
  case 144 : // toggle block normale/colonne
   if (flag&FL_BCOL) flag=flag&(FL_BCOL^0xffff);
   else flag|=FL_BCOL;
   block.type=flag&FL_BCOL; rdw|=36;
   if (block.type && block.cend==1023) { block.blk=0; block.cend=0; }
  break;
  case 145 : // set block begin
  {
   int ok=0;

   if (block.blk&2 && block.edtr==this)
   {
    blockok();
    if (lcorr<block.end || (lcorr==block.end && x<block.cend))
    { // posso estendere il blocco e lo visualizzo
     block.blk|=5; block.start=lcorr; block.cstart=x;
     block.type=flag&FL_BCOL;
     rdw|=4; ok=1;
    }
   }
   if (!ok)
   { // nisba, devo eliminare il precedente
    if (block.blk&4) rdw|=4;
    if (block.blk==7) { block.blk=0; winchk(block.edtr); }
    block.blk=1; block.edtr=this; block.start=lcorr; block.cstart=x;
    block.type=flag&FL_BCOL;
   }
  }
  break;
  case 146 : // set block end
  {
   int ok=0;

   if (block.blk&1 && block.edtr==this)
   {
    blockok();
    if (lcorr>block.start || (lcorr==block.start && x>block.cstart))
    { // posso estendere il blocco e lo visualizzo
     block.blk|=6; block.end=lcorr; block.cend=x;
     block.type=flag&FL_BCOL;
     rdw|=4; ok=1;
    }
   }
   if (!ok)
   { // nisba, devo eliminare il precedente
    if (block.blk&4) rdw|=4;
    if (block.blk==7) { block.blk=0; winchk(block.edtr); }
    block.blk=2; block.edtr=this; block.end=lcorr; block.cend=x;
    block.type=flag&FL_BCOL;
   }
  }
  break;
  case 147 : // toggle block line drag si/no
   if (flag&FL_BLDR) flag=flag&(FL_BLDR^0xffff);
   else
   {
    rdw|=4;
    if (block.blk==7) { block.blk=0; winchk(block.edtr); }
    flag|=FL_BLDR; block.blk=7; block.type=0; block.edtr=this;
    block.start=block.end=lcorr; block.cstart=0; block.cend=1023;
   }
   flag=flag&(FL_BCDR^0xffff);
   rdw|=32;
  break;
  case 148 : // toggle block char drag si/no
   if (flag&FL_BCDR) flag=flag&(FL_BCDR^0xffff);
   else
   {
    if (block.blk&4) rdw|=4;
    if (block.blk==7) { block.blk=0; winchk(block.edtr); }
    flag|=FL_BCDR; block.blk=7; block.edtr=this; block.type=flag&FL_BCOL;
    block.start=block.end=lcorr; block.cstart=block.cend=x;
   }
   flag=flag&(FL_BLDR^0xffff);
   rdw|=32;
  break;
  case 150 : // cresize==resize o start block
   {
    int ok=0;
    int os,oe;

    os=oe=-1;
    if (block.blk==7 && block.edtr==this)
    {
     os=block.start; oe=block.end;
     blockok();
     if ((block.end==plc && block.cend==px) ||
         (block.end+1==plc && block.cend>lbuf && px==0))
     { block.end=lcorr; block.cend=x; }
     else
     if (block.start==plc && block.cstart==px)
     { block.start=lcorr; block.cstart=x; }
     else
     if (block.type && block.end==plc && block.cstart==px)
     { block.end=lcorr; block.cstart=x; }
     else
     if (block.type && block.start==plc && block.cend==px)
     { block.start=lcorr; block.cend=x; }
     else
     if (block.start==plc)
     { block.start=lcorr; block.cstart=x; }
     else
     if (block.end==plc)
     { block.end=lcorr; block.cend=x; }
     else
     {
      block.blk=0;
      putdaa(block.start,block.end);
      ok=1;
     }
    } else ok=1;
    if (ok) // start block
    {
     if (block.blk==7) { block.blk=0; winchk(block.edtr); }
     block.blk=7; block.start=plc; block.cstart=px; block.cend=x;
     block.edtr=this; block.type=flag&FL_BCOL;
     block.end=lcorr; block.cend=x;
    }
    blockok();
    if (os!=-1) { rdda=block.start<?os; rda=(block.end>?oe); }
    else { rdda=block.start; rda=block.end; }
    rdw|=256;
   }
  break;
  case 151 : // toggle block hide
  case 156 : // force block hide
   if (block.blk&4)
   {
    block.blk&=3;
    flag=flag&(FL_BLDR^0xffff); flag=flag&(FL_BCDR^0xffff);
    rdw|=36;
   }
   else
   if (cmd==151)
    if ((block.blk&3)==3) { block.blk|=4; if (block.edtr==this) rdw|=4; }
  break;
  case 152 : px=x; plc=lcorr; break; // set point per il block resize
  case 153 : // set normale
   flag=flag&(FL_BCOL^0xffff); rdw|=32;
   if (block.type) rdw|=4;
   block.type=0;
  break;
  case 154 : // set no block line drag
   flag=flag&(FL_BLDR^0xffff); puthdr(4);
  break;
  case 155 : // set no block char drag
   flag=flag&(FL_BCDR^0xffff); puthdr(4);
  break;
  case 161 : // del to EOL
   if (lbuf>x)
   {
    if (delbuf->add(buffer+x,lbuf-x,3)) confirm(s_oom,0,yda,config.col[5]);
    memset(buffer+x,lbuf-x,' ');
    lbuf=x; flag|=FL_RMODIF; rdw|=129;
   }
  break;
  case 162 : // toggle showmatch
   if (config.showmatch) config.showmatch=0;
   else { config.showmatch=1; config.shwtab=1; }
   rdw|=32;
  break;
  case 163 : config.showmatch=0; rdw|=32; break; // reset showmatch
  case 164 : // toggle indent
   if (config.indent) config.indent=0; else config.indent=1;
   rdw|=32;
  break;
  case 165 : config.indent=1; rdw|=32; break; // set indent
  case 166 : // block unselect
   flag=flag&(FL_BLDR^0xffff); flag=flag&(FL_BCDR^0xffff);
   block.blk=0; rdw|=36;
  break;
  case 167 : // tolower
   if (isupper(buffer[x]))
   { buffer[x]=tolower(buffer[x]); rdw|=1; flag|=FL_RMODIF; }
  break;
  case 168 : // toupper
   if (islower(buffer[x]))
   { buffer[x]=toupper(buffer[x]); rdw|=1; flag|=FL_RMODIF; }
  break;
  case 169 : // flip case
   if (islower(buffer[x]))
   { buffer[x]=toupper(buffer[x]); rdw|=1; flag|=FL_RMODIF; }
   else
   if (isupper(buffer[x]))
   { buffer[x]=tolower(buffer[x]); rdw|=1; flag|=FL_RMODIF; }
  break;
  case 170 : ex=31; break; // shell param
  case 171 : ex=32; break; // hide window
  case 172 : // toggle showtab
   if (config.shwtab && !docmode && !config.showmatch && !config.colormode)
    config.shwtab=0; else config.shwtab=1;
   rdw|=32;
  break;
  case 173 : config.shwtab=1; rdw|=32; break; // set showtab
  case 174 : // toggle doc
   if (docmode) docmode=0; else { docmode=1; config.shwtab=1; }
   rdw|=32|4;
  break;
  case 175 : docmode=1; config.shwtab=1; rdw|=32; break; // set doc
// case 175 : soft enter vedi case 135
  case 177 : // centra la riga
  {
   internal=1;
   rdw|=execcmd(180); // deindento
   int t;
   t=(config.rmargin-config.lmargin-lbuf)/2+config.lmargin;
   if (t>0 && t+lbuf<1023)
   {
    memmove(buffer+t,buffer,lbuf); lbuf+=t;
    memset(buffer,' ',t); x+=t; if (x>1023) x=1023;
    rdw|=1; flag|=FL_RMODIF;
   }
   internal=0;
  }
  break;
  case 178 : // riga a destra
  {
   internal=1;
   rdw|=execcmd(180); // deindento
   int t;
   t=(config.rmargin-lbuf);
   if (t>0 && t+lbuf<1023)
   {
    memmove(buffer+t,buffer,lbuf); lbuf+=t;
    memset(buffer,' ',t); x+=t; if (x>1023) x=1023;
    rdw|=1; flag|=FL_RMODIF;
   }
   internal=0;
  }
  break;
  case 179 : // riga a sinistra
  {
   internal=1;
   rdw|=execcmd(180); // deindento
   int t;
   t=config.lmargin-1;
   if (t>0 && t+lbuf<1023)
   {
    memmove(buffer+t,buffer,lbuf); lbuf+=t;
    memset(buffer,' ',t); x+=t; if (x>1023) x=1023;
    rdw|=1; flag|=FL_RMODIF;
   }
   internal=0;
  }
  break;
  case 180 : // deindenta (toglie gli spazi in testa alla riga
  {
   int t;

   for (t=0; buffer[t]==' ' && t<lbuf; t++);
   lbuf-=t; memmove(buffer,buffer+t,lbuf);
   x-=t; if (x<0) x=0;
   rdw|=9; flag|=FL_RMODIF;
  }
  break;
  case 181 : // elimina le modifiche alla riga attuale
   started(); rdw|=1;
  break;
  case 182 : // sposta il cursore al margine sinistro
   x=config.lmargin; rdw|=8;
  break;
  case 183 : config.vars|=1; break;
  case 184 : config.vars|=2; break;
  case 185 : config.vars|=4; break;
  case 186 : config.vars|=8; break;
  case 187 : config.vars^=1; break;
  case 188 : config.vars^=2; break;
  case 189 : config.vars^=4; break;
  case 190 : config.vars^=8; break;
  case 191 : // insert control code
  {
   int n,i,t;
   dialog number;
   char str[80];

   str[0]='\0';
   number.ask("Insert hex code of character(s)",yda,str);
   n=0; t=0;

   for (i=0; str[i]!='\0'; i++)
   {
    n=(n<<4)|htoi(str[i]); t++;
    if (t==2)
    {
     int stabs=config.tabexpr;
     config.tabexpr=0;

     execch(n&0xff);
     config.tabexpr=stabs;
     t=0; n=0;
    }
   }
  } break;
  case 192:
  {
   macrostop=1;
   gomenu(0,0);
   macrostop=0;
  } break; // menu
  case 193 : // set autosave
  {
   dialog number;
   char str[80];

   str[0]='\0';
   if (number.ask("Set autosave to (sec):",yda,str)==0)
   {
    timeval tv;
    int i,t,ts;

    config.autosave=atol(str);
    gettimeofday(&tv,(struct timezone *)0);

    if (config.autosave) ts=config.autosave+tv.tv_sec; else ts=0;

    t=edt.rtot(); // numero di editor
    edt.nseek(0);
    for (i=0; i<t; i++)
    {
     editor *myed=(*((editor **)edt.rdati()));
     if (myed->flag&FL_MODIF) myed->timeouttosave=ts;
     else                     myed->timeouttosave=-1;
     edt.next();
    }
   }
  } break;
  case 194 : // m_gotowin
  {
   mouse.togox=mouse.x; mouse.togoy=mouse.y; mouse.win=1;
  } break;
  case 195 : // m_gotozed
  {
   mouse.togox=mouse.x; mouse.togoy=mouse.y; mouse.win=0;
  } break;
  case 196 : break; // l_noop
 }

 if (flag&FL_BLDR && lcorr!=block.end)
 { block.end=lcorr; rdw|=4; } // devo aggiornare il blocco - line drag mode

 if (flag&FL_BCDR) // devo aggiornare il blocco - char drag mode
 {
  if (block.cend!=x) { rdw|=1; block.cend=x; if (block.type) rdw|=4; }
  if (block.end!=lcorr) { rdw|=4; block.end=lcorr; }
 }

 if (!config.shwtab)
 {
  if (chkcor!=corr)
   x=vtor(vx,0);
 }

 if (!internal)
 {
  if (!binary && docmode && (oldcor!=lcorr || (rdw&512) || pari==-1))
  { // ricalcolo inizio/fine paragrafo
   int npari,npare;

   npari=lcorr;
   elem *scorr=corr;
   while (irprev(scorr) && isoftret(irprev(scorr)))
   { scorr=irprev(scorr); npari--; }

   npare=lcorr;
   scorr=corr; if (flag&FL_BUFFER) setsoft(soft);
   while (irnext(scorr) && isoftret(scorr))
   { scorr=irnext(scorr); npare++; }

   if (npari!=pari || npare!=pare || (rdw&512))  // se cambia il paragrafo
   {
    if (pari!=-1)
    {
     int rda,ra;

     rda=pari; ra=pare; pari=pare=-1;
     // deseleziono il paragrafo
     if (!(rdw&4))
     {
      if (!redraw) { putdaa(rda,npari); putdaa(npare,ra); }
      else           putdaa(rda,ra);
     }
    }

    pari=npari; pare=npare;         // nuovo paragrafo
    if (!redraw)
    {
     if (!(rdw&4))
     {
      if (rdw&2) // 259=256+1+2
      { rdw&=~259; rdda=(lcorr<?pari); rda=rtot(); rdw|=256; }
      else
      if (rdw&256)
      { rdw&=~1; rdda=(rdda<?pari); rda=(rda>?pare); }
      else
      if (rdw&1)
      { rdw&=~1; rdda=(lcorr<?pari); rda=(lcorr>?pare); rdw|=256; }
      if (((rdw&512) && !(rdw&263)) || !(rdw&263))
      { rdw&=~512; rdda=pari; rda=pare; rdw|=256; }
     }
    } else pare=pari=-1;
   }
   else if (rdw&512) { rdw&=~512; rdda=pari; rda=pare; rdw|=256; }
  }

       if (rdw&4) putpag(); // redraw page
  else if (rdw&2) putres(vidy,corr,lcorr); // redraw il resto
  else if (rdw&256) putdaa(rdda,rda);
  else if (rdw&1) putelm(0,0,vidy); // redraw elemento

  if (rdw&8) setcur();
  int h=0;
  if (rdw&16) h=8;
  if (rdw&32) h|=4;

  if (!todraw) h|=3;

  puthdr(h);
  if (rdw&128) okdelbuf();

  if (!binary && !ex && config.shwtab && config.showmatch)
  { // devo mostrare il match
   char ch,fw,bw;
   int nx=0;

   if (flag&FL_BUFFER)
   { if (lbuf>x) ch=buffer[x]; else ch=' '; }
   else
   { if (rlung()>x) ch=*(rdati()+x); else ch=' '; }

   fw=bw=0;

   if (colorize==3)
   {
    switch(ch)
    {
     case '(' : fw='('; bw=')'; nx=1; break;//)
     case '<' : fw='<'; bw='>'; nx=1; break;
     case '[' : fw='['; bw=']'; nx=1; break;//]
     case '{' : fw='{'; bw='}'; nx=1; break;//}((
     case ')' : fw=')'; bw='('; nx=0; break;//)
     case '>' : fw='>'; bw='<'; nx=0; break;//[[
     case ']' : fw=']'; bw='['; nx=0; break;//{{]
     case '}' : fw='}'; bw='{'; nx=0; break;//}
    }
   }
   else
   {
    switch(ch)
    {
     case '(' : fw='('; bw=')'; nx=1; break;//)
     case '[' : fw='['; bw=']'; nx=1; break;//]
     case '{' : fw='{'; bw='}'; nx=1; break;//}((
     case ')' : fw=')'; bw='('; nx=0; break;//)[[
     case ']' : fw=']'; bw='['; nx=0; break;//]{{
     case '}' : fw='}'; bw='{'; nx=0; break;//}
    }
   }

   if (fw)
   {
    char *p;
    int l,y,pos,cnt;
    elem *scorr;

    if (nx)
    {
     if (flag&FL_BUFFER) { p=buffer+x; l=lbuf-x; }
     else { p=rdati()+x; l=rlung()-x; }
     scorr=corr; y=vidy; pos=x; cnt=0;
     if (!nx) l=x;
     while(y<=ya && scorr)
     {
      int i;

      for (i=l; i>0; i--)
      {
       ch=*p;
       if (ch==fw) cnt++;
       if (ch==bw) { cnt--; if (cnt==0) break; }
       p++; pos++;
      }
      if (cnt==0) break;
      scorr=irnext(scorr); y++;
      if (scorr) { l=irlung(scorr); p=irdati(scorr); pos=0; }
     }
    }
    else
    {
     if (flag&FL_BUFFER) p=buffer+x; else p=rdati()+x;
     l=x+1;
     scorr=corr; y=vidy; pos=x; cnt=0;
     while(y>yda && scorr)
     {
      int i;

      for (i=l; i>0; i--)
      {
       ch=*p;
       if (ch==fw) cnt++;
       if (ch==bw) { cnt--; if (cnt==0) break; }
       p--; pos--;
      }
      if (cnt==0) break;
      scorr=irprev(scorr); y--;
      if (scorr) { l=irlung(scorr); p=irdati(scorr)+l-1; pos=l-1; }
     }
    }
    if (cnt==0 && ex==0 && pos>=vidx && pos<(vidx+config.maxx))
    {
     short int *pnt;

     match.x=pos; match.y=y;

     pnt=vbuf+y*config.maxx+pos-vidx;
     match.save=*pnt; *pnt=(match.save&0xff)|(config.col[14]<<8);
     if (config.ansi)
     { match.ox=pos-vidx; match.oy=y; astatok(y,y,pos-vidx,pos-vidx); }
    }
   }
  }
 }
 if (ex) update();
 if (internal) ex=rdw;
 return(ex);
}

/***************************************************************************/
// ritorna la parola puntata dal cursore - serve per la shell con parametri
// str max 80 char

void editor::word(char *str)
{
 char *dt=rdati();
 int dl=rlung(),l=0,pos;

 pos=x;

 str[l]='\0';

 while(pos>0 && isword(dt[pos-1])) pos--;

 while(pos<dl && isword(dt[pos]) && l<78) str[l++]=dt[pos++];
 str[l]='\0';
}

/***************************************************************************/

char itoh(int i);

/***************************************************************************/
// cancella il file in memoria. serve per la clipboard

void editor::reset(void)
{
 list::eraseall();
 list::startup();
 flag=0;
}

/***************************************************************************/
/* gestione blocco dal file corrente alla clipboard o al delbuffer con
   eventuale cancellazione dell'originale
  m :
  1 : del originale
  2 : del originale -> delbuf
  4 : originale -> clipboard
*/

int editor::doblockclip(int m)
{
 #define FED block.edtr
 #define CLP clip.edtr
 int svlc;   // save lcurrent
 int err,r;

 err=0; r=0;

 blockok();
 if (CLP->rtot()==1 && CLP->prima->lung==0) CLP->reset();

 if (!block.type && m&3 && block.start<FED->lcorr
   && block.start>FED->lcorr-(FED->vidy-FED->yda)
   && block.end<FED->lcorr+(FED->ya-FED->vidy)) // sposto il curs. vert.
 {  // SE cancello e inizio blocco prima della posiz. del curs. attuale
    // e parte del blocco e' a video
  int decy;

  if (FED->lcorr>block.end)
  {
   decy=(int)(block.end-block.start);//+1;
   if (block.cstart==0 && block.cend==1023) decy++;
  }
                       else decy=(int)(FED->lcorr-block.start);
  if (FED->vidy-decy<FED->yda) FED->vidy=FED->yda;
  FED->vidy-=decy;
 }

 svlc=FED->lcorr; err=!FED->nseek(block.start); // seek del from
 if (m&4)
 {
  clip.blktype=block.type;
  clip.cstart=block.cstart; clip.cend=block.cend;
 }

 if (err==0)
 { // seek-ato OK
  if (block.type || block.start==block.end)
  { // a blocco
   int i;
   int l;

//   clip.blktype=block.type;
   l=block.cend-block.cstart; // in l la larghezza del blocco
   for (i=block.end-block.start+1; (i>0 && err==0); i--)
   {
    int ll;

    lbuf=FED->rlung(); if (m&6) memcpy(buffer,FED->rdati(),lbuf);
    if (block.cend>lbuf) { ll=lbuf-block.cstart; if (ll<0) ll=0; } else ll=l;
    // in ll ho la larghezza effettiva del blocco nella linea corrente
    if (m&4 && err==0) err=CLP->add(buffer+block.cstart,ll,3);
    if (m&2 && err==0) err=delbuf->add(buffer+block.cstart,ll,3);
    if (m&3 && err==0)
     if (!block.type && block.cend==1023 && block.cstart==0)
     { FED->remove(); if (svlc>block.start) svlc--; }
     else
     if (ll>0)
     { // se devo cancellare la sorgente
      if (!(flag&FL_SOVR))
      {
       lbuf-=ll;
       memmove(buffer+block.cstart,buffer+block.cend,lbuf-block.cstart);
      }
      else
      {  // modalita' sovrascrittura
       memset(buffer+block.cstart,' ',block.cend-block.cstart);
      }
      err=FED->subst(buffer,lbuf|FED->softret());
     }
    if (err==0)  FED->next();
   }
   if (m&3 && FED->x>=block.cstart && !block.type && block.end==svlc)
    if (FED->x<=block.cend) FED->x=block.cstart; else FED->x-=l;
  }
  else
  { // multiriga
   int ll;
   int sub=0;

   lbuf=FED->rlung(); if (m&6) memcpy(buffer,FED->rdati(),lbuf);
   if (block.cstart>lbuf) ll=0; else ll=lbuf-block.cstart;
// ll e' la larghezza reale del blocco nella riga
   if (m&4 && err==0) err=CLP->add(buffer+block.cstart,ll|FED->softret(),3);
   if (m&2 && err==0) err=delbuf->add(buffer+block.cstart,ll,3);
   if (m&3 && err==0) // se devo cancellare la sorgente
    if (ll>0 && lbuf!=ll) // se devo modificare la riga
    { lbuf-=ll; err=FED->subst(buffer,lbuf|FED->softret()); }
    else
    if (block.cstart>0) { if (lbuf==ll) err=FED->subst("",0); }
    else { FED->remove(); sub++; }
   if (!sub) FED->next();
   sub=0;

   int tot=block.end-block.start+1;

   if (err==0 && tot>2)
   { // copio la parte centrale
    for (int i=tot-2; (i>0 && err==0); i--)
    {
     if (m&4) err=CLP->add(FED->rdati(),FED->rlung()|FED->softret(),3);
     if (m&2 && err==0) err=delbuf->add(FED->rdati(),FED->rlung(),3);
     if (m&3 && err==0) { FED->remove(); sub++; }
     else FED->next();
    }
   }

   if (err==0) // adesso, la coda
   {
    lbuf=FED->rlung(); if (m&6) memcpy(buffer,FED->rdati(),lbuf);
    if (block.cend<lbuf) ll=block.cend; else ll=lbuf;

    if (m&4) CLP->add(buffer,ll,3);
    if (m&2) delbuf->add(buffer,ll,3);
    if (m&3)
     if (ll>0 && lbuf>ll) // se devo cancellare la sorgente
     {
      memmove(buffer,buffer+block.cend,lbuf-ll);
      lbuf-=ll; err=FED->subst(buffer,lbuf|FED->softret());
     }
     else
     if (block.cend<1023) err=FED->subst("",0); else { FED->remove(); sub++; }
   }

   if (m&3) // sposto il cursore
   {
    if
     ((svlc==block.start && FED->x>block.cstart)
   || (svlc>block.start && svlc<block.end)) FED->x=block.cstart;

    if (svlc==block.end)
    {
     if (FED->x<=block.cend) FED->x=block.cstart;
     else FED->x-=block.cend-block.cstart;
    }

    if (svlc>block.start)
     if (svlc<block.end) svlc=block.start;
     else svlc-=sub+1;

    if (block.cstart>0) // devo riunire le due righe
    {
     int sx; // save x

     sx=FED->x;
     FED->prev();
     FED->x=block.cstart;

     started(); // canc
     { // devo riunire due righe
      elem *p;

      if (FED->x>lbuf) lbuf=FED->x;
      int ll=(((MAXLINE-FED->x)<?(FED->irlung(FED->crnext()))))>?0;
      memcpy(buffer+FED->x,FED->irdati(FED->crnext()),ll);
      lbuf+=ll;
      soft=FED->isoftret(FED->crnext());
      p=FED->corr; FED->next(); FED->remove();
      if (p!=corr) FED->prev();
      FED->flag|=FL_RMODIF; FED->setmodif();
      r|=18;
     }
     FED->update();
     FED->x=sx;
    }
   }
  }

  if (m&3)
  {
   elem *scorr;
   int slcorr;

   FED->inseek(&scorr,slcorr,block.start);

   if (block.type)
   {
    colcheck(scorr,block.end-block.start+1>?0);
   } else colcheck(scorr);

   r|=44; block.blk=0; flag=flag&((FL_BLDR|FL_BCDR)^0xffff);
  }
  FED->nseek(svlc);
  if (err==1) confirm(s_oom,0,yda,config.col[5]);
  if (m&3) { FED->flag|=FL_RMODIF; FED->setmodif(); r|=32; }
  if (CLP==this) r|=36;

 }
 okdelbuf();

 if (m&3) winchk(FED);
 if (m&2) winchk(delbuf);
 if (m&4) winchk(clip.edtr);

 return(r);
}

/***************************************************************************/
// muove la clipboard all'interno del file
// se m!=0 cancello la clipboard man mano che la uso

int editor::doclipins(int m)
{
 #define CLP clip.edtr
 int svlc;   // save lcurrent
 int err,r;
 int lines;

 if (x>1022) return(0);

 err=0; r=0;

 svlc=lcorr;
 lines=CLP->rtot()-clip.pastefrom;
 if (lines<0) { clip.pastefrom=0; lines=CLP->rtot(); }
 if (lines==0) return(0);

 CLP->nseek(clip.pastefrom);

 block.type=clip.blktype;

 flag=flag&(~FL_BCOL); if (block.type) flag|=FL_BCOL;

 // inserimento blocco a righe
 if (clip.blktype && clip.cend==1023) return(0);
 else
 if (clip.blktype==0 && (lines>1 || (lines==1 && clip.cend==1023)))
 {
  int i,bend;

  if (block.blk==7) { block.blk=0; winchk(block.edtr); }

  block.edtr=this;
  block.type=0;
  block.blk=7;
  block.cstart=x; block.cend=1023;
  block.start=block.end=lcorr; bend=lcorr;

  if (clip.cend<1023) lines--; // l'ultima riga dopo

  if (x>0) // sistemo la prima riga del blocco, se e' in mezzo
  {
   int lmax;

   lbuf=rlung(); memcpy(buffer,rdati(),lbuf);
   memset(buffer+lbuf,' ',1024-lbuf);
   if (x<lbuf) err=add(buffer+x,(lbuf-x)|softret(),2);
          else err=add("",0|softret(),2);

   lbuf=x;

   lmax=CLP->rlung(); if (lbuf+lmax>1023) lmax=1024-lbuf;
   memcpy(buffer+lbuf,CLP->rdati(),lmax); lbuf+=lmax;
   if (err==0) err=subst(buffer,lbuf|CLP->softret());

   if (m) CLP->remove(); else CLP->next();
   lines--;
   next();
   bend++;
  }

  for (i=0; err==0 && i<lines; i++) // sistemo la parte centrale
  {
   add(CLP->rdati(),CLP->rlung()|CLP->softret(),1);
   if (m) CLP->remove(); else CLP->next();
   bend++;
  }

  if (clip.cend<1023) // ultima riga ?
  {
   int lmax;

   lbuf=CLP->rlung(); memcpy(buffer,CLP->rdati(),lbuf);
   block.cend=lbuf;
   lmax=rlung(); if (lbuf+lmax>1023) lmax=1024-lbuf;
   memcpy(buffer+lbuf,rdati(),lmax); lbuf+=lmax;
   if (err==0) err=subst(buffer,lbuf|softret());
   if (m) CLP->remove(); else CLP->next();
   bend++;
  }

  block.end=bend-1;
  r|=36;
 }
 else
 { // a blocco o singola riga
  int i; // indice per i loop
  int larg; // larghezza del blocco

  if (block.blk==7) { block.blk=0; winchk(block.edtr); }
  block.blk=7; block.edtr=this;
  block.start=block.end=lcorr;

  elem *scorr=corr;
  for (i=0; i<lines-1; i++)
  {
   if (irnext(scorr)==0) err=add("",0,3);
   scorr=irnext(scorr);
  }

  larg=clip.cend-clip.cstart; if (larg+x>1023) larg=1023-x;

  for (i=0; err==0 && i<lines && err==0; i++)
  {
   int lclip;

   lbuf=rlung(); memcpy(buffer,rdati(),lbuf);
   memset(buffer+lbuf,' ',1023-lbuf);

   lclip=CLP->rlung(); if (lclip+x>1023) lclip=1023-x;

   if (!((flag&FL_SOVR))) // inserimento
   {
    if (lbuf>x)
    {
     int smax;

     smax=lbuf-x; if (x+smax>1023) smax=1023-x;
     memmove(buffer+x+larg,buffer+x,smax);
    }
    memcpy(buffer+x,CLP->rdati(),lclip);
    if (lclip<larg) memset(buffer+x+lclip,' ',larg-lclip);

    if (x<lbuf) lbuf+=larg; else lbuf=x+lclip;
   }
   else
   {                      // sovrascrittura
    memcpy(buffer+x,CLP->rdati(),lclip);
    if (x+lclip>lbuf) lbuf=x+lclip;
   }

   err=subst(buffer,lbuf|softret());

   if (m) CLP->remove(); else CLP->next();
   next();
  }

  block.cstart=x; block.cend=x+larg;
  prev();
  block.blk=7; block.end=lcorr;

  r|=36;
 }

 FED->nseek(svlc);
 if (err==1) confirm(s_oom,0,yda,config.col[5]);
 flag|=FL_RMODIF; setmodif();

 if (block.blk)
 {
  elem *scorr;
  int slcorr;

  FED->inseek(&scorr,slcorr,block.start);

  colcheck(scorr,block.end-block.start+1>?0);
 }

 if (m) winchk(clip.edtr);

 r|=16;
 return(r);
}

/***************************************************************************/
// esporta la clipboard nella stringa selectstr
// se m!=0 cancello la clipboard man mano che la uso
// se my!=0 metto l'intestazione

#ifdef X11

void editor::exportclip(int m,int my)
{
 sele.pos=0; sele.len=0;
 delete sele.str;

 sele.len=10240;
 sele.pos=0;
 sele.str=new char [sele.len];

 #define CLP clip.edtr
 int i;

 if ((clip.pastefrom)>(CLP->rtot())) clip.pastefrom=0;
 CLP->nseek(clip.pastefrom);

 if (clip.blktype && my)
 {
  sprintf(sele.str,"%c%c%08x%c%08x%c%c",
          255,200,clip.cstart,201,clip.cend,202,10);
  sele.pos=strlen(sele.str);
 }

 int tot=CLP->rtot()-clip.pastefrom;

 for (i=tot; i>0; i--)
 {
  if (sele.pos+CLP->rlung()+10>sele.len)
  { sele.len+=10240; sele.str=(char *)realloc(sele.str,sele.len); }

  memcpy(sele.str+sele.pos,CLP->rdati(),CLP->rlung()); sele.pos+=CLP->rlung();
  if (CLP->softret()) sele.str[sele.pos++]=config.softret;
  if (i>1 || clip.cend==1023) sele.str[sele.pos++]=10; // EOL
  if (m) CLP->remove(); else CLP->next();
 }

 XSetSelectionOwner(display,XA_PRIMARY,zed_win,sele.time);

 winchk(clip.edtr);
}

#endif

/***************************************************************************/
// importa la clipboard

#ifdef X11

void editor::importclip(int window,int property)
{
 Atom actual_type;
 int actual_format,i;
 int nitems,bytes_after,nread;
 unsigned char *data,*d2;
 unsigned char buffer[1050];
 int bpos;

 if (property==None) return;

 nread=0; clip.blktype=0; clip.cstart=0; clip.cend=0;
 if (CLP->rtot()==1 && CLP->prima->lung==0) CLP->reset();

 if ((clip.pastefrom)>(CLP->rtot())) clip.pastefrom=0;
 CLP->nseek(clip.pastefrom);

 do
 {
  if (XGetWindowProperty(display,(unsigned int)window,(unsigned int)property,
      nread/4,1024,True,
      AnyPropertyType,&actual_type,&actual_format,
      (unsigned long *)&nitems,(unsigned long *)&bytes_after,
      (unsigned char **)&data)!=Success) return;
  if (actual_type!=XA_STRING) return;
  bpos=0;
  d2=data;

  for (i=0; i<nitems; i++,d2++)
  {
   int sr=0;

   if (*d2==10 || bpos>1020)
   {
    if (buffer[0]==255 && buffer[1]==200 && buffer[10]==201 && buffer[19]==202)
    {
     clip.blktype=1;
     clip.cstart=0; clip.cend=0;
     int j;

     for (j=0; j<8; j++) clip.cstart=(clip.cstart<<4)|htoi(buffer[ 2+j]);
     for (j=0; j<8; j++) clip.cend  =(clip.cend  <<4)|htoi(buffer[11+j]);

     if (clip.cend>1023 || clip.cend<0 || clip.cstart>1023 || clip.cstart<0)
     { clip.blktype=1; clip.cstart=0; clip.cend=0; }
    }
    else
    {
     if (bpos>0 && buffer[bpos-1]==config.softret) { sr=0x4000; bpos--; }
     CLP->add((char *)buffer,bpos|sr,3);
     if (!clip.blktype) clip.cend=1023;
    }
    bpos=0;
   } else buffer[bpos++]=*d2;
  }
  nread+=nitems;
  XFree(data);
 } while (bytes_after>0);

 if (bpos)
 {
  CLP->add((char *)buffer,bpos,3);
  if (!clip.blktype) clip.cend=bpos;
 }

 winchk(clip.edtr);

}

#endif

/***************************************************************************/
/* carica/inizializza file in memoria
 ritorna:
 0:ok
 1:error reading file
 3:fatal error, no memory to load file
 5:error opening file

 all=0, un solo file normale
 all=1, ho tanti file da caricare
 all=2, open in binary mode
 all=4, open in readoly mode
*/

int editor::loadfile(char *nome,int all)
{
 int err,spl,ok,l;
 int tabexpand,tabcheck;
 char *buf,*save;
 short int l2;
 mfile mf;

 err=0;

 colorize=0; tabexpand=0; tabcheck=0;

 buf=nome+strlen(nome)-1;

 if (all==2) { all=0; binary=1; }
 if (all==4) { all=0; readonly=1; tabexpand=1; }

 switch(config.colormode)
 {
  case 1: // auto
  {
   int l=strlen(nome);

   if (l>5)
   {
    if (memcmp(nome+l-5,".html",4)==0) colorize=3;
    if (memcmp(nome+l-5,".java",4)==0) colorize=6;
   }
   if (l>4 && colorize==0)
   {
    if (memcmp(nome+l-4,".cpp",4)==0) colorize=2;
    if (memcmp(nome+l-4,".tex",4)==0) colorize=4;
    if (memcmp(nome+l-4,".htm",4)==0) colorize=3;
    if (memcmp(nome+l-4,".jav",4)==0) colorize=6;
   }
   if (l>3 && colorize==0)
   {
    if (memcmp(nome+l-3,".cc",3)==0) colorize=2;
   }
   if (l>2 && colorize==0)
   {
    if (memcmp(nome+l-2,".c",2)==0) colorize=2;
    else
    if (memcmp(nome+l-2,".H",2)==0) colorize=2;
    else
    if (memcmp(nome+l-2,".h",2)==0) colorize=2;
    else
    if (memcmp(nome+l-2,".C",2)==0) colorize=2;
   }
   if (colorize==0 && strstr(nome,config.strs+config.strmail)!=0)
    colorize=5;
  } break;
  case 2: colorize=2; break; // force C++
  case 3: colorize=3; break; // force html
  case 4: colorize=4; break; // force tex
  case 5: colorize=5; break; // force mail
  case 6: colorize=6; break; // force java
 }

 if (config.doc==2)
 {
  int l=strlen(nome);

  docmode=0;
  if (l>4 && memcmp(nome+l-4,".doc",4)==0) docmode=1;
  if (colorize==5) docmode=1;
 }

 switch (config.tabexpl)
 {
 // case 0 : no expand
  case 1 : tabexpand=1; break;
  case 2 :
  {
   if (!strstr(nome,"Makefile") && !strstr(nome,"makefile")) tabexpand=1;
  } break;
  case 3 :
  {
   if (!strstr(nome,"Makefile") && !strstr(nome,"makefile")) tabcheck=1;
  } break;
 }

 save=new char[config.maxx*2+2];
 saveline(yda,save);
 strcpy(filename,nome);
 puthdr(16);
 putstr("Loading file...          ",25,0,yda,config.col[6]);
 vsetcur(0,0);
 okstatus(yda);

 flag=0; spl=0; ok=0;

 switch(mopen(nome,&mf))
 {
  case 1 : vputstr("New file",8,yda,config.col[6]); ok=2; flag|=FL_NEWFILE|FL_SCRITT;
           break;
  case 2 : err=5; ok=1; break;
  case 3 : err=3; ok=1; break;
 }

 if (ok==0 && binary) // open in binary?
 {
  colorize=0;
  binary=1;
  flag|=FL_SOVR;
 } else binary=0; // se new file, force mode !binary

 if (err==0)
 {
  if (ok==0 || ok==2) { while(prima) remove(); }
  if (ok==0) // open OK to READ file
  {
   int count=0,totcount=0;
   buf=0;

   if (binary==0)
   {
    while(!mread(&buf,1023,&err,&l,&mf))
    {
     l2=l&0xfff;
     if (l2==1023) { spl=1; l2--; l--; }

     if (tabcheck && !tabexpand)
     {
      if (memchr(buf,'\t',l2))
      {
       char ch;

  ch=confirm("File contains tab(s), expand (y/n)? ","yn",yda,config.col[6]);
       if (ch=='y') tabexpand=1;
       tabcheck=0;
      }
     }

     if (tabexpand)
     {
      char lbuf[1030];
      memcpy(lbuf,buf,l2);
      tabexp(lbuf,&l2); l=l2|(l&0xfc00);
      if (add(lbuf,l,3)) err=2;
     } else if (add(buf,l,3)) err=2;
     count++; totcount++;
     if (count>1023)
     {
      count=0; putint(totcount,7,16,yda,config.col[6]); okstatus(yda);
     }
     buf=0;
    }
   }
   else
   {
    while((l=read(mf.h,mf.buf,BUFFSIZE))>0)
    {
     int pos=0;

     buf=mf.buf;

     while(l>0)
     {
      l2=(l>16)?16:l;

      if (add(buf+pos,l2,3)) err=2;
      count++; totcount++;
      if (count>1023)
      {
       count=0; putint(totcount,7,16,yda,config.col[6]); okstatus(yda);
      }
      l-=l2; pos+=l2;
     }
    }
   }

   mclose(&mf);
   corr=prima; lcorr=0;

   if (!binary) colcheck(prima,-1); // passo tutto il file per i colori

   vputstr("Load Ok",7,yda,config.col[6]);
  }
  if (!corr) if (add("",0,3)) err=3;
 }

 if (err) restline(yda,save);

 if (err==10) err=1;
 if (err==2)
  if (all)
  {
   confirm("Out Of Memory, not all file loaded, hit ESC",0,yda,config.col[5]);
  }
  else
  {
   confirm("Out Of Memory, partial file loaded, hit ESC",0,yda,config.col[5]);
   part=1;
  }
 if (spl)
 {
  char ch;

  ch=confirm("File seems binary, reopen in hex mode (y/n) ?","yn",yda,config.col[6]);

  if (ch=='y')
  {
   char nome2[1024];
   int il;

   strncpy(nome2,nome,1023);
   il=strlen(nome2);

   return(loadfile(nome2,2));
  }
  confirm("Long lines splitted, hit ESC",0,yda,config.col[6]);
 }
 if (spl || err==2) vputstr("Load NOk",8,yda,config.col[6]);

 if (err==2 && !all) err=0;
 delete save;

 if (err==0)
 {
  stat(filename,&stato);
  stato.st_nlink=0;
  stato.st_atime=0;
 }

 return(err);
}

/***************************************************************************/
/* crea l'help editor
 ritorna:
 0:ok
 1:error reading file
 3:fatal error, no memory to load file
 5:error opening file
*/

int editor::createhelp(void)
{
 int i,nf,err,l,count=0,totcount=0;
 char *nomi,*cn,*buf,*save;
 mfile mf;
 char row[1024];

 docmode=0; readonly=1;
 filename[0]='>'; filename[1]='H'; filename[2]='\0';

 save=new char[config.maxx*2+2];
 saveline(yda,save);

 puthdr(16);
 putstr("Creating help...         ",25,0,yda,config.col[6]);

 err=0;

 vsetcur(0,0);
 okstatus(yda);

 flag=0;

 nomi=strdup(configs);

 for (i=0,nf=1; nomi[i]!='\0'; i++)
 {
  if (nomi[i]==(char)127) { nomi[i]='\0'; nf++; }
 }

 for (i=0,cn=nomi; i<nf; i++,cn=cn+strlen(cn)+1)
 {
  int ok;

  strcpy(row,"File "); strcat(row,cn); strcat(row,":");

  ok=mopen(cn,&mf);

  if (ok) strcat(row,"error accessing file.");
  add(row,strlen(row),3);
  if (ok) continue;
  buf=0;

  while(!mread(&buf,1023,&err,&l,&mf))
  {
   char *p;

   l&=0xfff;

   memcpy(row,buf,l); row[l]='\0';
   p=strstr(row,";*");

   if (p && !(p>row && *(p-1)=='\\'))
   {
    strcpy(row,p+2);
    add(row,strlen(row),3);
    count++; totcount++;
    if (count>1023)
    { count=0; putint(totcount,7,16,yda,config.col[6]); okstatus(yda); }
   }
  }
  mclose(&mf);
 }

 corr=prima; lcorr=0;

 vputstr("Help Ok",7,yda,config.col[6]);
 if (!corr) if (add("",0,3)) err=3;

// if (err) restline(yda,save);

 delete save;

 return(err);
}

/***************************************************************************/
// salva il file
// return 0 se all ok

int editor::savefile(void)
{
 int file,err,eoll,eoll2;
 char *save,*eols,eols2[80];
 char *buff;
 unsigned int pos,max,ln;
 int cx,cy;
 struct stat miostato;

 // memory full, partial file loaded etc... are part of the
 // "things of the past" (i.e. msdos with 640K limit) maybe one day I'll
 // remove them

 if (readonly) return(0);

 vsavecur(&cx,&cy);
 if (part)
 {
  char ret=confirm("Partial file loaded, confirm save (y/n/Esc)","yn",yda>?0,
                   config.col[6]);
  if (ret!='y') return(0);
  part=0;
 }

 stat(filename,&miostato);
 miostato.st_nlink=0;
 miostato.st_atime=0;
/*
 if (memcmp(&miostato,&stato,sizeof(struct stat)))
 {
  char ret=confirm("File on disk changed, confirm save (y/n/Esc)","yn",yda>?0,
                   config.col[6]);
  if (ret!='y') return(0);
 }
*/
 save=new char[config.maxx*2+2];
 max=0xffff; pos=0;

 while (((buff=(char *)malloc(max))==0) && max>1030) max>>=1;

 max-=1028;

 err=0;
 eols=config.strs+config.streol; eoll=strlen(eols);
 strcpy(eols2+1,eols); eoll2=eoll+1; *eols2=config.softret;
 if (flag&FL_BUFFER) update(); // updato il buffer

 if (yda>-1)
 {
  saveline(yda,save);
  if (buff) putstr("Saving file...          ",24,0,yda,config.col[6]);
  else      putstr("Saving file without buffer...",29,0,yda,config.col[6]);
  vsetcur(0,0);
  okstatus(yda);
 }
 if (config.bak)
 {
  char fbak[200];

  strcpy(fbak,filename);
  #ifdef UNIX
   strcat(fbak,"~");
  #else
   char *p;

   if ((p=strchr(fbak,'.'))!=0) *p='\0';
   strcat(fbak,".bak");
  #endif

  if (my_access(fbak))
  {
   rename(fbak,"$$J$KW$L.B~J");
   rename(filename,fbak);
   rename("$$J$KW$L.B~J",filename);
  }
  else rename(filename,fbak);
 }

 if (flag&FL_APPEND)
 {
  file=my_open(filename,2);
  if (file>0) lseek(file,0,SEEK_END);
  flag&=~FL_APPEND;
 }
 else
 {
  file=my_open(filename,1);
 }

 if (file>0)
 {
  elem *scorr;
  int count=0,totcount=0;

  scorr=prima;
  if (docmode)
   while (scorr && err==0) // document mode
   {
    if (buff)
    {
     if (pos>max) { if (write(file,buff,pos)==-1) err=1; pos=0; }
     ln=irlung(scorr);
     memcpy(buff+pos,irdati(scorr),ln); pos+=ln;
     if (isoftret(scorr))
     { memcpy(buff+pos,eols2,eoll2); pos+=eoll2; }
     else
     { memcpy(buff+pos,eols,eoll); pos+=eoll; }
     count++; totcount++;
     if (yda>-1 && count>1023)
     { count=0; putint(totcount,7,16,yda,config.col[6]); }
    }
    else
    {
     if (write(file,irdati(scorr),irlung(scorr))==-1) err=1;
     if (isoftret(scorr))
     { if (write(file,eols2,eoll2)==-1) err=1; }
     else
     { if (write(file,eols,eoll)==-1) err=1; }
    }
    scorr=irnext(scorr);
   }
  else
   if (!binary)
    while (scorr && err==0) // normal mode
    {
     if (buff)
     {
      if (pos>max) { if (write(file,buff,pos)==-1) err=1; pos=0; }
      ln=irlung(scorr);
      memcpy(buff+pos,irdati(scorr),ln); pos+=ln;
      memcpy(buff+pos,eols,eoll); pos+=eoll;
      count++; totcount++;
      if (count>1023 && yda>-1)
      { count=0; putint(totcount,7,16,yda,config.col[6]); okstatus(yda); }
     }
     else
     {
      if (write(file,irdati(scorr),irlung(scorr))==-1) err=1;
      if (write(file,eols,eoll)==-1) err=1;
     }
     scorr=irnext(scorr);
    }
    else
    while (scorr && err==0) // binary mode
    {
     if (buff)
     {
      if (pos>max) { if (write(file,buff,pos)==-1) err=1; pos=0; }
      ln=irlung(scorr);
      memcpy(buff+pos,irdati(scorr),ln); pos+=ln;
      count++; totcount++;
      if (count>1023 && yda>-1)
      { count=0; putint(totcount,7,16,yda,config.col[6]); okstatus(yda); }
     }
     else
     {
      if (write(file,irdati(scorr),irlung(scorr))==-1) err=1;
     }
     scorr=irnext(scorr);
    }

  if (buff && pos>0)
  { if (write(file,buff,pos)==-1) err=1; pos=0; }
  if (file>0) close(file);
 }
 else err=2;
 if (yda>-1) restline(yda,save);
 free(buff);

 switch(err)
 {
  case 0 : flag=flag&((FL_MODIF|FL_RMODIF)^0xffff); break;
  case 1 :
   confirm("Error writing output file, hit ESC",0,yda>?0,config.col[5]);
  break;
  case 2 :
   confirm("Error opening output file, hit ESC",0,yda>?0,config.col[5]);
  break;
 }
 if (yda>-1) puthdr(4);
 vrestcur(cx,cy);
 delete save;

 if (err==0)
 {
  stat(filename,&stato);
  stato.st_nlink=0;
  stato.st_atime=0;
 }

 return(err);
}

/***************************************************************************/
/* typ=0 avanti, case sens
   typ=1 avanti, case insens, sstr e' lower
   typ=2 indietro, case sens
   typ=3 indietro, case insens, sstr e' lower

str=riga in cui cercare, sstr=stringa da cercare
len=lunghezza "          slen=lunghezza "
start=posizione di partenza all'interno di str end=pos di fine
*/

int editor::meminstr(char *str,char *sstr,int len,int slen,int start,int end,int typ)
{
 char *p;
 int i=0;

 len--;
 if (len>end) len=end;
 if ((len-start+1)<slen || slen<1) return(-1); // riga troppo corta

 len-=slen-1; // il -1 e' un len++;

 if (len<end) end=len;

 if (typ==0)
 { // avanti, case sensitive
  p=str+start; i=start;

  while (i<=end)
  {
   while(*p!=*sstr && i<end) { p++; i++; }        // cerco il primo char
   if (memcmp(p,sstr,slen)==0) end=-1; else { p++; i++; }
  }
  if (end>-1) i=-1;
 }
 else if (typ==1)
 { // avanti, case insensitive
  p=str+start; i=start;

  while (i<=end)
  {
   while((unsigned)tolower(*p)!=(unsigned int)*(unsigned char *)sstr && i<end)
   { p++; i++; }// cerco il primo char
   if (memicmp(p,sstr,slen)==0) end=-1; else { p++; i++; }
  }
  if (end>-1) i=-1;
 }
 else if (typ==2)
 { // indietro, case sensitive
  p=str+(end); i=end;

  while (i>=start)
  {
   while(*p!=*sstr && i>start) { p--; i--; }        // cerco il primo char
   if (memcmp(p,sstr,slen)==0) start=1024; else { p--; i--; }
  }
  if (start<1024) i=-1;
 }
 else if (typ==3)
 { // indietro, case insensitive
  p=str+(end); i=end;

  while (i>=start)
  {
   while((unsigned)tolower(*p)!=(unsigned int)*(unsigned char *)sstr && i>start)
   { p--; i--; }        // cerco il primo char
   if (memicmp(p,sstr,slen)==0) start=1024; else { p--; i--; }
  }
  if (start<1024) i=-1;
 }
 return(i);
}

/***************************************************************************/
/* typ=0 avanti, case sens
   typ=1 avanti, case insens, sstr e' lower
   typ=2 indietro, case sens
   typ=3 indietro, case insens, sstr e' lower

str=riga in cui cercare, sstr=stringa da cercare
len=lunghezza "          slen=lunghezza "
start=posizione di partenza all'interno di str end=pos di fine
*/

int editor::reginstr(char *str,int len,int *slen,int start,int end,int notbol,regex_t *regex)
{
 char buf[1025];
 int l;
 regmatch_t fnd;

 l=len<?end; l-=start;

 if (l<1 || l>1023) return(-1);

 memcpy(buf,str+start,l); buf[l]='\0';

 if (regexec(regex,buf,1,&fnd,notbol?REG_NOTBOL:0)) return(-1);

 *slen=fnd.rm_eo-fnd.rm_so;

 return(fnd.rm_so+start);
}

/***************************************************************************/

void editor::find(int modo)
{
 // modo=0 interac. search, =1 normal search, =2 sostituzione, =3 rifare

 int flag,lss; // lss=lunghezza della search string
 char opt[80],str[80],strto[80];
 unsigned int nsubst,nsubc;
 dialog dopt;
 regex_t regex;

 if (lulti==0) return; // se non ci sono righe, esco
 nsubst=nsubc=0; str[0]='\0'; strto[0]='\0'; flag=0;

 if (s_str) strcpy(str,s_str);
 if (s_strto) strcpy(strto,s_strto);

 switch(modo)
 {
  case 0 : // caso isearch
  {
   if (s_stropti) strcpy(opt,s_stropti); else
    strcpy(opt,config.strs+config.strifnddef);
   str[0]='\0'; // azzero la stringa
   if (dopt.ask("Interactive search option(s): (Ign_case Sel_text Back Entire)",yda,opt))
    return;
   putstr(" ISEARCH ",0,19,yda,config.col[6]);
   delete s_stropti; s_stropti=strdup(opt);
  }
  break;
  case 1 : // caso search
  {
   int ok;

   if (s_stropts) strcpy(opt,s_stropts); else
    strcpy(opt,config.strs+config.strfnddef);

   do
   {
    char testa[80];
    int ret;

    ok=0;

    sprintf(testa,"Search string ([%s]:Tab to change):",opt);

    ret=dopt.ask(testa,yda,str,1);

    if (ret==1 || !str[0]) return;
    if (ret==0) break;

    ret=dopt.ask("Search option(s): (Ign_case Sel_text Back Entire Regex regeX)",yda,opt,1);
    opt[20]='\0';
    if (ret==1) return;
    if (ret==0) break;
   } while(1);

   delete s_stropts;
   s_stropts=new char[strlen(opt)+1];
   if (s_stropts) strcpy(s_stropts,opt);
  }
  break;
  case 2 : // caso subst
  {
   if (s_stropta) strcpy(opt,s_stropta); else
    strcpy(opt,config.strs+config.strrpldef);
   if (dopt.ask("Search string:",yda,str)) return;
   if (!str[0]) return;
   if (dopt.ask("New string:",yda,strto)) return;
   if (dopt.ask("Replace option(s): (Ign_case Sel_text Back Entire All No_confirm)",yda,opt))
    return;
   delete s_stropta;
   s_stropta=new char[strlen(opt)+1];
   if (s_stropta) strcpy(s_stropta,opt);
   flag=512;
  }
  break;
  case 3 : // caso "rifare"
  {
   if (!s_str || !str[0]) return;
   flag=s_flag; // eredito i flags
  }
  break;
/*
 i: senza cerca con riconoscimento del case, con il case non ha importanza
 s: cerca all'interno del blocco selezionato se il blocco e' nel buffer
    corrente:
    -se il cursore e' dentro il blocco, la ricerca parte dalla posizione
     del cursore
    -se il cursore e' fuori dal blocco, oppure e' stato specificato "e",
     la ricerca parte dall'inizio del blocco (o dalla fine se "b")
    In ogni caso la ricerca finisce alla fine (inizio se "b") del blocco
 b: senza cerca in avanti: dal cursore/inizio_file/inizio_blocco fino al
     fine_file/fine_blocco, altrimenti cerca all'indietro: dal
     cursore/fine_file/fine_blocco fino all' inizio_file/inizio_blocco
 e: seleziona "tutto": puo' essere tutto il file o tutto il blocco, a seconda
     se e' stato selezionato "s" o no. altrimenti la ricerca parte dal
     cursore, tranne il caso in cui e' stato dato "s" e il cursore e' fuori
     dal blocco.
 a: nel caso di sostituzioni, cicla finche' tutte le sostituzione sono state
     effettuate.
 n: nel caso di sostituzioni, non chiede conferma prima di eseguire la/le
     sostituzioni
 r: cerca con una regular expression
 x: cerca con una regular expression extesa
// w: scrive il numero di sostituzioni eseguite man mano che vengono effettuate
//     se sostituzione e "an"
*/
 }

 lss=strlen(str);

 if (modo!=3)
 {
  s_flag=0;
  strlwr(opt);
  if (strchr(opt,'i')) flag|=1; // ignore case
  if (strchr(opt,'s') && block.edtr==this && block.blk==7) flag|=2; //sel_text
  if (strchr(opt,'b')) flag|=4; // back
  if (strchr(opt,'e')) flag|=8; // entire scope
  if (modo==2 && strchr(opt,'a')) flag|=16; // change all
  if (modo==2 && strchr(opt,'n')) flag|=32; // no confirm
//  if (modo==2 && strchr(opt,'w')) flag|=256; // write number of changes 128 e 64 interni
  if (modo==2 && (flag&16) && (flag&32)) flag|=64; // novideo
//  if (((flag&256)==256) && (flag&(16+32))!=(16+32)) flag-=256;
  if ((flag&(16|32))==(16|32)) flag|=256;
  if (modo>0)
  {
   if (strchr(opt,'r')) flag|=2048; // entire scope
   if (strchr(opt,'x')) flag|=4096; // entire scope
  }
 }

 if (flag&1) strlwr(str); // lower la stringa di ricerca

 if ((flag&2) && (block.edtr!=this || block.blk!=7)) flag-=2;

 if (modo<3)
 {
  delete s_str;   s_str=strdup(str);
  delete s_strto; s_strto=0;

  if (modo==2) s_strto=strdup(strto);
 }

 if (modo==3) if (s_flag&512) modo=2; else modo=1;

 s_flag=flag; if (s_flag&8) s_flag-=8;

 if (flag&(2048|4096))
 {
  int ret;

  if (flag&4)
  {
   confirm("Backward Regex not allowed - ESC",0,yda,config.col[6]);
   return;
  }

  if ((ret=regcomp(&regex,str,
                   ((flag&4096)?REG_EXTENDED:0)|((flag&1)?REG_ICASE:0))))
  {
   char err[80];
   regerror(ret,&regex,err,80);
   confirm(err,0,yda,config.col[6]);

   regfree(&regex);

   return;
  }
 }

 /**** posiziono il cursore ****/

 elem *scorr=corr;
 int slcorr=lcorr,savelstart=lcorr;;
 int xx=x;

 // scorr,slcorr=puntatore nella lista delle righe
 // xx=posizione da cui cominciare la ricerca(da 0 a strlen(riga))

 // corr,lcorr=puntatore corrente nell'editor
 // x=posizione corrente del cursore

 if (flag&2)
 { // se in block go to block start o end
  if ((lcorr<block.start || (lcorr==block.start && x<block.cstart))
    ||(lcorr>block.end   || (lcorr==block.end   && x>block.cend  ))
    ||(block.type && (x<block.cstart || x>block.cend)))
  flag|=8; // se fuori blocco, attivo il goto blocco (entire)
 }

 if (flag&8) // se entire
 {
  if (flag&2) // se block
  { // goto inizio o fine blocco
   elem *save=corr;
   int lsave=lcorr;

   if (flag&4) { nseek(block.end);   xx=block.cend; }
   else        { nseek(block.start); xx=block.cstart; }

   scorr=corr; slcorr=lcorr; corr=save; lcorr=lsave;
  }
  else
  { // se entire non blocco parto da inizio o fine
   if (flag&4) { scorr=ulti; slcorr=lulti; xx=irlung(scorr); }
          else { scorr=prima; slcorr=0; xx=0; }
  }
 }

 /**** fine posizionamento ****/

 // da qui scorr punta all'elemento, slcorr e' il numero d'ordine xx start pos

 int ret,d,ln,f;
 char *dt;

 f=1; // prima volta o no per la scritta "searching..."
 do
 {
  ret=0;
  if (modo==0)
  { // se interattivo, controlla la tastiera e aggiorna.
   d=0;
   switch(exectst(&d))
   {
    case 1 : // in arrivo un carattere
    {
     if (flag&1) d=tolower(d);
     str[lss++]=d; str[lss]='\0'; // aggiorno la stringa di ricerca
     ret=1; // vai con la ricerca
//     f=1;
     if (f!=1) xx-=lss-1;
    }
    break;
    case 2 :
    {
     switch(d)
     {
      case 12 : ret=255; break; // escape
      case 42 : ret=1;   break; // repeat ricerca
      case 135: // backspace
      {
       if (lss>0)
       {
        lss--;
        str[lss]='\0';
        xx-=lss+1;
        if (lss==0)
        {
         x=xx;
         putpag();      // scrivo la pagina
         setcur();
        } else ret=1;
       }
      } break;
     }
    } break;
    // se esc, abort, torno 255 e se search_again vai
   }
  } else ret=1; // se non interattivo, vai con la ricerca

  if (ret==1)
  { // eseguo la ricerca
   int back,blk;
   int type,start,end,notbol=0;

   if ((!(flag&256)) || (f==1))
   { putstr("Searching...",0,0,yda,config.col[6]); f=0; }
   type=(flag&1)|((flag&4)>>1);
   back=flag&4; blk=flag&2; // ignore case, avanti/ind, in block
   dt=irdati(scorr); ln=irlung(scorr);

   if (back) { start=0; end=xx-1; } else { start=xx; end=1024; notbol=1; }

   if (blk)
   {
    if ( back && (slcorr==block.start || block.type))
    { start=block.cstart; notbol=0; }
    if (!back && (slcorr==block.end   || block.type)) end=  block.cend;
   }

   int fn; // search return value

   do
   { // ciclo di ricerca
    if (flag&(2048|4096))
     fn=reginstr(dt,ln,&lss,start,end,notbol,&regex);
    else
     fn=meminstr(dt,str,ln,lss,start,end,type);

    if (fn==-1) // se non trovato, prendo la riga successiva
    {
     if (back)
     { // se scorro all'indietro
      if ((slcorr>0 && !blk) || (blk && slcorr>block.start))
      {
       scorr=irprev(scorr); slcorr--;
       start=0; end=1024;
       if (blk && slcorr==block.start) start=block.cstart;
       if (blk && block.type) { start=block.cstart; end=block.cend; }
       dt=irdati(scorr); ln=irlung(scorr);
      }
      else dt=0; // fine del file
     }
     else
     { // scorro in avanti
      if ((slcorr<lulti && !blk) || (blk && slcorr<block.end))
      {
       scorr=irnext(scorr); slcorr++;
       start=0; end=1024; notbol=0;
       if (blk && slcorr==block.end) end=block.cend;
       if (blk && block.type) { start=block.cstart; end=block.cend; }
       dt=irdati(scorr); ln=irlung(scorr);
      }
      else dt=0;
     }
    }
   } while(dt!=0 && fn==-1); // ciclo finche' matcha!

   if (dt)
   { // trovato !
    if (back) xx=fn; else xx=fn+lss;
    //setcur(); // posiziono il cursore
    if (!(flag&64))
    {
     short int *start=0;
     int i,ml,mls;
     char col2;

     gotol(slcorr); pari=pare=-1; // vado alla riga giusta
     x=xx;
     putpag();      // scrivo la pagina
     setcur();
     col2=config.col[12];

     ml=(rtov(fn)-vidx);
     mls=lss;

     if (ml<0) { mls+=ml; ml=0; }

     start=vbuf+ml+config.maxx*vidy;

     for (i=0; i<mls; i++)
     {
      *start=(*start&0xff)|(col2<<8);
      start++;
     }

     redraw=1;
     if (config.ansi) astatok(vidy,vidy,fn-vidx,fn-vidx+lss);
    }
    if (modo==2)
    { // parte la sostituzione
     int ch;

     if (!(flag&32)) ch=confirm("Replace (y/n/a/ESC) ?","yna",yda,config.col[6]);
     else ch='y';
     if (ch=='a') { flag|=32|16; ch='y'; if (modo==2 && (flag&16)) flag|=64; }
     if (ch==-1) ret=255; // ESC
     if (ch=='y')
     { // eseguo la sostituzione
      int tol;

      tol=strlen(strto);
      lbuf=irlung(scorr);
      memcpy(buffer,irdati(scorr),lbuf);
      memmove(buffer+fn+tol,buffer+fn+lss,lbuf-fn-lss);
      memcpy(buffer+fn,strto,tol);
      lbuf+=tol-lss; xx+=tol-lss; if (back) xx++;

      if (isubst(&scorr,buffer,lbuf))
      { confirm(s_oom,0,yda,config.col[5]); ret=255; }
      if (!(flag&64)) puthdr(8);
      nsubst++; nsubc++;
      if (!(flag&64)) { putelm(0,0,vidy); setcur(); }
      else
      if (flag&256)
      {
       if (nsubc>1023)
       {
        nsubc=0;
        putint(nsubst,7,0,yda,config.col[6]);
        putstr("Subst",0,7,yda,config.col[6]);
       }
      }
      setmodif();
     }
    }
   }
   else
   {
    if (modo!=2 || !(flag&16))
     confirm("String not found - press ESC",0,yda,config.col[6]);
    ret=255;
   }
   if (!(flag&64)) puthdr(3);
  }
//  if (!(flag&64)) xx=x; //??? cos'era?? generava un bug!
  if (modo==1 || (modo==2 && !(flag&16))) ret=255;
//  if (ret!=255) { xx++; if (!(flag&64)) x++; }
 } while (ret!=255);
 puthdr(4);

 if (modo==2)
 {
  inseek(&scorr,slcorr,savelstart);

  if (colcheck((flag&4)?corr:scorr,-1,str,strto)) putpag();
 }

 if (modo==2 && (flag&16))
 {
  if (flag&64) putpag();
  putint(nsubst,7,0,yda,config.col[6]);
  putstr("Subst",0,7,yda,config.col[6]);
  todraw=1;
 } else puthdr(3);

 if (flag&(2048|4096)) regfree(&regex);
}

/***************************************************************************/

void editor::setpipe(int fd)
{
 if (pipefd) { close(pipefd); delete ppbuf; }
 pipefd=fd;
 ppbuf=new char [config.maxx+1]; pppos=0;
}

/***************************************************************************/
// costruttore: inizializzazioni varie

editor::editor(int yda1,int ya1)
{
 int i;

 for (i=0; i<4; i++) { posiz[i]=0; posizx[i]=0; }
 flag=0; todraw=redraw=0;
 yda=yda1; ya=ya1; vidx=x=vx=0; vidy=yda+1;
 oldcor=-1; soft=0; pari=pare=-1;
 part=0; pipefd=0; ppbuf=0; pppos=0; colorize=0;
 binary=0; mailquote=0; docmode=config.doc;
 readonly=0;
}

/***************************************************************************/

editor::~editor(void)
{
 if (yda>-1)
 {
  putstr("Quitting file...          ",25,0,yda,config.col[6]);
  vsetcur(0,0);
  okstatus(yda);
 }
 if (block.edtr==this) block.blk=0;
 if (pipefd) close(pipefd);
 delete ppbuf; ppbuf=0;
 pipefd=0; pppos=0;
 delete mailquote; mailquote=0;
}

/***************************************************************************/

int editor::add(char *data,int len,int modo)
{
 int part2=len&0xfc00;
 len=len&0x3ff;

 if (config.nospace && !binary && this!=clip.edtr)
 {
  while(len>0 && (data[len-1]==' ' || data[len-1]==9)) len--;
 }

 return(list::add(data,len|part2,modo));
}

/***************************************************************************/

int editor::subst(char *data,int len)
{
 int part2=len&0xfc00;
 len=len&0x3ff;

 if (config.nospace && !binary && this!=clip.edtr)
 {
  while(len>0 && (data[len-1]==' ' || data[len-1]==9)) len--;
 }

 return(list::subst(data,len|part2));
}

/***************************************************************************/
// indico che e' un buffer sotto

void editor::back(void)
{
 yda=-1; ya=-1;
}

/***************************************************************************/

void editor::setmodif(void)
{
 if (config.autosave)
 {
  timeval tv;

  gettimeofday(&tv,(struct timezone *)0);
  timeouttosave=config.autosave+tv.tv_sec;
 }
 flag|=FL_MODIF;
}

/***************************************************************************/
// loop centrale per i tasti e comandi

int editor::looptst(void )
{
 int ret,d;

 do
 {
  ret=0;
  d=0;
  switch(exectst(&d))
  {
   case 1 :
    execch(d);
   break;
   case 2 :
    ret=execcmd(d);
   break;
  }
 } while (!ret);

 return(ret);
}

/***************************************************************************/

