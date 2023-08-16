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

/* edvis.cc - gestione del singolo editor/buffer, seconda parte*/

#define COL_TO_SCROLL config.coltoscroll
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

/***************************************************************************/

int editor::coloracpp(unsigned short int *buf,char *str,
                      int vidx,int len,int ss)
{
 static char *res="5while3for2if4case6switch6struct5union5class"
"3int4char4long5float6double8volatile4auto6static4void8unsigned5short"
"6return6public7private3new6delete7default6inline6sizeof4this6friend"
"7typedef8register6extern7virtual5const6signed4enum3asm9protected"
"8operator4else2do5break8continue4goto8template3try5catch5throw";
 static char *symbols=" \t!?=;:,.*-+/%&|><()[]{}^~";

//                    01234567890123456789012
  static char *color=";180b3486599;77777023:0"; // ':'=10 ';'=11

 int stato=0;
 int pos=0;
 int resta=len;
 char *p=str;
 int ava;
 int next=0;
 int save1=-1; // salva il numero di caratteri del reserved word
 int save2=-1; // conta i caratteri in number mode
 char c=0;

 switch(ss) // caso speciali di start state
 {
  case 1: ava=resta-1; stato=1; break; // caso preprocessore
  case 2: stato=2; break; // caso interno stringa
  case 3: stato=3; break; // caso interno commento "/* */"
  case 4: ava=resta-1; stato=18; break; // caso interno commento "//"
 }

 if (pos>=len && stato==3) next=3; // caso linea vuota

 while(pos<len)
 {
  c=*p;
  ava=0;

  switch(stato)
  {
   case 0:  // normale = puo' essere qualunque cosa
   case 7:  // ultimo " della stringa
   case 11: // ultimo ' della stringa
   case 19: // era un simbolo unico
   case 20: // normale seguente = non puo' essere un preprocessore
   case 21: // era un errore
   {
    char p1='\xff';
    ava=0;

    if (resta>1)
    {
     p1=*(p+1);
     if (c=='/' && p1=='/') { ava=resta-1; stato=18; break; }
     else
     if (c=='/' && p1=='*')
     {
      if (resta==2) next=3; // caso linea == solo "/*"
      ava=2; stato=3; break;
     }
     else
     if (c=='0' && p1=='x') { save2=0; ava=2; stato=9; break; }
    }
    if (c==' ' || c=='\t') { ava=1; break; }
    if (resta==1 && c=='0') { ava=1; stato=6; break; }
    if (c=='0')
    {
     if (resta>1 && (p1=='L' || p1=='l' || p1=='u' || p1=='U'))
     { ava=1; save2=0; stato=6; break; }
     else
     { ava=1; save2=0; stato=8; break; }
    }
    if (c>='1' && c<='9') { save2=0; ava=1; stato=6; break; }
    if (c=='#' && stato==0) { ava=resta-1; stato=1; break; }
    if (c=='"') { ava=1; stato=2; break; }
    if (c=='\''){ ava=1; stato=10; break; }
    if (strchr(symbols,c)!=0)
    { ava=1; stato=19; break; }
    {
     char *i=res;

     while(*i!='\0')
     {
      if (memcmp(p,i+1,(int)(*i-'0'))==0)
      { ava=(int)(*i-'0'); save1=ava; stato=12; break; }
      i+=(int)(*i-'0')+1;
     }
     if (ava) break;
    }
    stato=5; ava=1;
   } break;
   case 1: // ultimo carattere del preprocessore
   {
    ava=1; if (c=='\\') next=1;
   } break;
   case 2: // interno di stringa
   {
    if (resta>1 && c=='\\') { ava=2; break; }
    if (resta==1 && c=='\\') { ava=1; next=2; break; }
    if (c=='"') { ava=1; stato=7; }
    ava=1;
   } break;
   case 3: // interno di un commento
   {
    if (resta>1)
    { if (c=='*' && *(p+1)=='/') { ava=2; stato=22; break; } }
    else { ava=1; next=3; break; }
    ava=1;
   } break;
   case 22: stato=20; break; // fine commento
   case 12: // carattere dopo la reserved word
   {
    if ((c>='a' && c<='z') || (c>='A' && c<='Z') ||
        (c>='0' && c<='9') || c=='_') { ava=-save1; stato=5; break; }

    if (strchr(symbols,*p)!=0) stato=20; else { stato=21; ava=1; }
   } break;
   case 5: // identifier
   {
    if ((c>='a' && c<='z') || (c>='A' && c<='Z') ||
        (c>='0' && c<='9') || c=='_') { ava=1; break; }
    if (strchr(symbols,*p)!=0) stato=20; else { stato=21; ava=1; }
   } break;
   case 6: // decimal integer
   {
    if (c>='0' && c<='9') { ava=1; break; }
    if (c=='.') { ava=1; stato=13; break; }
    if (c=='e' || c=='E') { ava=1; stato=15; break; }
    if (strchr(symbols,*p)!=0) { stato=20; break; }
    if (c=='l' || c=='L' || c=='u' || c=='U') { ava=1; stato=19; break; }
    ava=-save2; stato=21;
   } break;
   case 8: // octal
   {
    if (c>='0' && c<='7') { ava=1; break; }
    if (save1==1 && c=='.') { ava=1; stato=13; break; }
    if (strchr(symbols,*p)!=0)
     if (save2==1) { ava=-1; stato=6; break; }
     else { stato=20; break; }
    if (c=='l' || c=='L' || c=='u' || c=='U') { ava=1; stato=19; break; }
    ava=-save2; stato=21;
   } break;
   case 9: // esadecimale
   {
    if ((c>='a' && c<='f') || (c>='A' && c<='F') ||
        (c>='0' && c<='9')) { ava=1; break; }
    if (strchr(symbols,*p)!=0) { stato=20; break; }
    if (c=='l' || c=='L' || c=='u' || c=='U') { ava=1; stato=19; break; }
    ava=-save2; stato=21;
   } break;
   case 10: // interno di apice
   {
    if (resta>1 && c=='\\') { ava=2; break; }
    if (c=='\'') { ava=1; stato=11; }
    ava=1;
   } break;
   case 13: // decimal+'.'
   {
    if (c>='0' && c<='9') { ava=1; stato=14; break; }
    if (strchr(symbols,*p)!=0) { stato=20; break; }
    if (c=='e' || c=='E') { ava=1; stato=15; break; }
    if (c=='f' || c=='F') { ava=1; stato=19; break; }
    ava=-save2; stato=21;
   } break;
   case 14: // decimale+'.'+...
   {
    if (c>='0' && c<='9') { ava=1; break; }
    if (c=='e' || c=='E') { ava=1; stato=15; break; }
    if (strchr(symbols,*p)!=0) { stato=20; break; }
    if (c=='f' || c=='F') { ava=1; stato=19; break; }
    ava=-save2; stato=21;
   } break;

   case 15: // float dopo 'e'
   {
    if ((c>='0' && c<='9') || c=='+' || c=='-') { ava=1; stato=16; break; }
    if (c=='f' || c=='F') { ava=1; stato=19; break; }
    ava=-save2; stato=21;
   } break;
   case 16: // float dopo 'e+'
   {
    if (c>='0' && c<='9') { ava=1; stato=17; break; }
    if (c=='f' || c=='F') { ava=1; stato=19; break; }
    ava=-save2; stato=21;
   } break;
   case 17: // float dopo 'e+4'
   {
    if (c>='0' && c<='9') { ava=1; break; }
    if (strchr(symbols,*p)!=0) { stato=20; break; }
    if (c=='f' || c=='F') { ava=1; stato=19; break; }
    ava=-save2; stato=21;
   } break;
   case 18: // ultimo carattere of line comment
   {
    ava=1;  if (*p=='\\') next=4;
   } break;
   default: stato=20; break;
  }

  if (ava<0)
  {
   ava=-ava; pos-=ava; resta+=ava; p-=ava; save2-=ava;
  }

  int col;

  if (buf)
  {
   col=(int)(unsigned char)(char)color[stato];
   col+=-'0'+20;
   col=config.col[col];

   if (col!=-1) col<<=8;
  } else col=-1;

  while(ava>0)
  {
   if (col>-1 && pos>=vidx && pos<vidx+config.maxx)
    buf[pos-vidx]=(buf[pos-vidx]&0xff)|col;
   pos++; ava--; resta--; p++; save2++;
  }

 }
 return(next);
}

/***************************************************************************/

int editor::colorajava(unsigned short int *buf,char *str,
                       int vidx,int len,int ss)
{
 static char *res="5while3for2if4case6switch5class\
4char4long5float6double8volatile6static4void5short\
6return6public7private3new7default\
5const9protected\
4else2do5break8continue4goto3try5catch4this6throws\
8abstract7boolean:implements5throw6import9transient\
4byte:instanceof7extends7finally5super6native\
<synchronized7package6String4null5final9interface3int";
 static char *symbols=" \t!?=;:,.*-+/%&|><()[]{}^~";

//                    01234567890123456789012
  static char *color=";180b3486599;77777023:0"; // ':'=10 ';'=11

 int stato=0;
 int pos=0;
 int resta=len;
 char *p=str;
 int ava;
 int next=0;
 int save1=-1; // salva il numero di caratteri del reserved word
 int save2=-1; // conta i caratteri in number mode
 char c=0;

 switch(ss) // caso speciali di start state
 {
//  case 1: ava=resta-1; stato=1; break; // caso preprocessore
  case 2: stato=2; break; // caso interno stringa
  case 3: stato=3; break; // caso interno commento "/* */"
  case 4: ava=resta-1; stato=18; break; // caso interno commento "//"
 }

 if (pos>=len && stato==3) next=3; // caso linea vuota

 while(pos<len)
 {
  c=*p;
  ava=0;

  switch(stato)
  {
   case 0:  // normale = puo' essere qualunque cosa
   case 7:  // ultimo " della stringa
   case 11: // ultimo ' della stringa
   case 19: // era un simbolo unico
   case 20: // normale seguente = non puo' essere un preprocessore
   case 21: // era un errore
   {
    char p1='\xff';
    ava=0;

    if (resta>1)
    {
     p1=*(p+1);
     if (c=='/' && p1=='/') { ava=resta-1; stato=18; break; }
     else
     if (c=='/' && p1=='*')
     {
      if (resta==2) next=3; // caso linea == solo "/*"
      ava=2; stato=3; break;
     }
     else
     if (c=='0' && p1=='x') { save2=0; ava=2; stato=9; break; }
    }
    if (c==' ' || c=='\t') { ava=1; break; }
    if (resta==1 && c=='0') { ava=1; stato=6; break; }
    if (c=='0')
    {
     if (resta>1 && (p1=='L' || p1=='l'))
     { ava=1; save2=0; stato=6; break; }
     else
     { ava=1; save2=0; stato=8; break; }
    }
    if (c>='1' && c<='9') { save2=0; ava=1; stato=6; break; }
//    if (c=='#' && stato==0) { ava=resta-1; stato=1; break; }
    if (c=='"') { ava=1; stato=2; break; }
    if (c=='\''){ ava=1; stato=10; break; }
    if (strchr(symbols,c)!=0)
    { ava=1; stato=19; break; }
    {
     char *i=res;

     while(*i!='\0')
     {
      if (memcmp(p,i+1,(int)(*i-'0'))==0)
      { ava=(int)(*i-'0'); save1=ava; stato=12; break; }
      i+=(int)(*i-'0')+1;
     }
     if (ava) break;
    }
    stato=5; ava=1;
   } break;
   case 1: // ultimo carattere del preprocessore
   {
    ava=1; if (c=='\\') next=1;
   } break;
   case 2: // interno di stringa
   {
    if (resta>1 && c=='\\') { ava=2; break; }
    if (resta==1 && c=='\\') { ava=1; next=2; break; }
    if (c=='"') { ava=1; stato=7; }
    ava=1;
   } break;
   case 3: // interno di un commento
   {
    if (resta>1)
    { if (c=='*' && *(p+1)=='/') { ava=2; stato=22; break; } }
    else { ava=1; next=3; break; }
    ava=1;
   } break;
   case 22: stato=20; break; // fine commento
   case 12: // carattere dopo la reserved word
   {
    if ((c>='a' && c<='z') || (c>='A' && c<='Z') ||
        (c>='0' && c<='9') || c=='_') { ava=-save1; stato=5; break; }

    if (strchr(symbols,*p)!=0) stato=20; else { stato=21; ava=1; }
   } break;
   case 5: // identifier
   {
    if ((c>='a' && c<='z') || (c>='A' && c<='Z') ||
        (c>='0' && c<='9') || c=='_') { ava=1; break; }
    if (strchr(symbols,*p)!=0) stato=20; else { stato=21; ava=1; }
   } break;
   case 6: // decimal integer
   {
    if (c>='0' && c<='9') { ava=1; break; }
    if (c=='.') { ava=1; stato=13; break; }
    if (c=='e' || c=='E') { ava=1; stato=15; break; }
    if (strchr(symbols,*p)!=0) { stato=20; break; }
    if (c=='l' || c=='L') { ava=1; stato=19; break; }
    ava=-save2; stato=21;
   } break;
   case 8: // octal
   {
    if (c>='0' && c<='7') { ava=1; break; }
    if (save1==1 && c=='.') { ava=1; stato=13; break; }
    if (strchr(symbols,*p)!=0)
     if (save2==1) { ava=-1; stato=6; break; }
     else { stato=20; break; }
    if (c=='l' || c=='L') { ava=1; stato=19; break; }
    ava=-save2; stato=21;
   } break;
   case 9: // esadecimale
   {
    if ((c>='a' && c<='f') || (c>='A' && c<='F') ||
        (c>='0' && c<='9')) { ava=1; break; }
    if (strchr(symbols,*p)!=0) { stato=20; break; }
    if (c=='l' || c=='L') { ava=1; stato=19; break; }
    ava=-save2; stato=21;
   } break;
   case 10: // interno di apice
   {
    if (resta>1 && c=='\\') { ava=2; break; }
    if (c=='\'') { ava=1; stato=11; }
    ava=1;
   } break;
   case 13: // decimal+'.'
   {
    if (c>='0' && c<='9') { ava=1; stato=14; break; }
    if (strchr(symbols,*p)!=0) { stato=20; break; }
    if (c=='e' || c=='E') { ava=1; stato=15; break; }
    if (c=='f' || c=='F' || c=='d' || c=='D') { ava=1; stato=19; break; }
    ava=-save2; stato=21;
   } break;
   case 14: // decimale+'.'+...
   {
    if (c>='0' && c<='9') { ava=1; break; }
    if (c=='e' || c=='E') { ava=1; stato=15; break; }
    if (strchr(symbols,*p)!=0) { stato=20; break; }
    if (c=='f' || c=='F' || c=='d' || c=='D') { ava=1; stato=19; break; }
    ava=-save2; stato=21;
   } break;

   case 15: // float dopo 'e'
   {
    if ((c>='0' && c<='9') || c=='+' || c=='-') { ava=1; stato=16; break; }
    if (c=='f' || c=='F' || c=='d' || c=='D') { ava=1; stato=19; break; }
    ava=-save2; stato=21;
   } break;
   case 16: // float dopo 'e+'
   {
    if (c>='0' && c<='9') { ava=1; stato=17; break; }
    if (c=='f' || c=='F' || c=='d' || c=='D') { ava=1; stato=19; break; }
    ava=-save2; stato=21;
   } break;
   case 17: // float dopo 'e+4'
   {
    if (c>='0' && c<='9') { ava=1; break; }
    if (strchr(symbols,*p)!=0) { stato=20; break; }
    if (c=='f' || c=='F' || c=='d' || c=='D') { ava=1; stato=19; break; }
    ava=-save2; stato=21;
   } break;
   case 18: // ultimo carattere of line comment
   {
    ava=1;  if (*p=='\\') next=4;
   } break;
   default: stato=20; break;
  }

  if (ava<0)
  {
   ava=-ava; pos-=ava; resta+=ava; p-=ava; save2-=ava;
  }

  int col;

  if (buf)
  {
   col=(int)(unsigned char)(char)color[stato];
   col+=-'0'+20;
   col=config.col[col];

   if (col!=-1) col<<=8;
  } else col=-1;

  while(ava>0)
  {
   if (col>-1 && pos>=vidx && pos<vidx+config.maxx)
    buf[pos-vidx]=(buf[pos-vidx]&0xff)|col;
   pos++; ava--; resta--; p++; save2++;
  }

 }

 return(next);
}

/***************************************************************************/

int editor::colorahtml(unsigned short int *buf,char *str,
                       int vidx,int len,int ss)
{
//                    0123456
  static char *color="  01222";

 int stato=0;
 int pos=0;
 int resta=len;
 char *p=str;
 int ava;
 int next=0;
 char c=0;

 switch(ss) // caso speciali di start state
 {
  case 1: stato=1; break; // deve apparire il comando
  case 2: stato=3; break; // siamo dentro i parametri
  case 3: stato=5; break; // caso interno stringa
  case 4: stato=4; break; // dopo un '='

  default: ss=0; break;
 }

 if (pos>=len) next=ss; // caso linea vuota

 while(pos<len)
 {
  c=*p;
  ava=0; next=0;

  switch(stato)
  {
   case 0: // sto cercando il '<'
   {
    ava=1; if (c=='<') { stato=1; next=1; }
   } break;
   case 1: // sto aspettando il comando
   {
    if (c==' ') { ava=1; next=1; }
    else if (c=='>') stato=0;
    else stato=2;
   } break;
   case 2: // in mezzo al comando
   {
    if (c=='>') stato=0;
    else if (c!=' ') { ava=1; next=2; }
    else stato=3;
   } break;
   case 3: // in mezzo ai parametri
   {
    ava=1;
    if (c=='>') { stato=0; break; }
    if (c=='=') { stato=4; next=4; break; }
    next=2;
   } break;
   case 4: // dopo un '='
   {
    ava=1;
    if (c=='"') { stato=5; next=3; break; }
    if (c==' ') { stato=3; next=2; break; }
    if (c=='>') { stato=0; break; }
    next=2;
   } break;
   case 5: // in mezzo ad una stringa
   {
    ava=1; next=3;
    if (c=='"') { stato=6; next=2; }
   } break;
   case 6: // ho passato l'ultimo '"' della stringa
   {
    stato=3; next=2;
   } break;

   default: stato=0; break;
  }

  int col;

  if (buf)
  {
   col=(int)(unsigned char)(char)color[stato];
   if (col==' ') col=-1; else { col+=-'0'+32; col=config.col[col]; }

   if (col!=-1) col<<=8;
  } else col=-1;

  while(ava>0)
  {
   if (col>-1 && pos>=vidx && pos<vidx+config.maxx)
    buf[pos-vidx]=(buf[pos-vidx]&0xff)|col;
   pos++; ava--; resta--; p++;
  }

 }
 return(next);
}

/***************************************************************************/

int editor::coloratex(unsigned short int *buf,char *str,
                      int vidx,int len,int ss)
{
//                    0123
  static char *color=" 220";

 int stato=0,math=0;
 int pos=0;
 int resta=len;
 char *p=str;
 int ava;
// int next=0;
 char c=0;

 switch(ss) // caso speciali di start state
 {
  case 1: math=1; break; // math $
  case 2: math=2; break; // math \[
  case 3: math=3; break; // math \(

  default: ss=0; break;
 }

// if (pos>=len) next=ss; // caso linea vuota

 while(pos<len)
 {
  c=*p;
  ava=0; //next=0;

  switch(stato)
  {
   case 0: // sto cercando il '\' o '$'
   {
    ava=1;

    if (c=='%') { ava=resta; stato=3; break; }
    if (math==0)
    {
     if (c=='$') { stato=1; math=1; break; }
     if (resta>1)
      if (c=='\\' && *(p+1)=='[') { ava=2; stato=1; math=2; break; }
      else
      if (c=='\\' && *(p+1)=='(') { ava=2; stato=1; math=3; break; }
    }
    else if (math==1 && c=='$') { stato=1; math=0; break; }
    else if (math==2 && resta>1 && c=='\\' && *(p+1)==']')
    { ava=2; stato=1; math=0; break; }
    else if (math==3 && resta>1 && c=='\\' && *(p+1)==')')
    { ava=2; stato=1; math=0; break; }

    if (resta>1 && c=='\\')
    {
     int c2=*(p+1);

     if (!(c2>='a' && c2<='z' || c2>='A' && c2<='Z')) { ava=2; stato=1; break; }
    }

    if (c=='\\') { stato=2; break; }
   } break;
   case 1: // devo uscire dal comando
   {
    stato=0;
   } break;
   case 2: // in mezzo al comando
   {
    int ok=0;

    if (!ok && !(c>='a' && c<='z' || c>='A' && c<='Z')) { stato=0; break; }
    ava=1;
   } break;

   default: stato=0; break;
  }

  int col;

  if (buf)
  {
   col=(int)(unsigned char)(char)color[stato];
   if (col==' ') if (math) col='1'; else col=-1;

   if (col>-1) { col+=-'0'+35; col=config.col[col]; }

   if (col!=-1) col<<=8;
  } else col=-1;

  while(ava>0)
  {
   if (col>-1 && pos>=vidx && pos<vidx+config.maxx)
    buf[pos-vidx]=(buf[pos-vidx]&0xff)|col;
   pos++; ava--; resta--; p++;
  }

 }
 return(math/*next*/);
}

/***************************************************************************/

int editor::coloramail(unsigned short int *buf,char *str,
                       int vidx,int len)
{
 int pos=0;
 int col=-1;
// int count=0;
 int ava=len;

 if (buf==0) return(0);

 for (int i=0; i<(20<?len); i++)
 {
//  if (str[i]!=' ' && str[i]!='>') { count++; if (count>4) break; }
  if (str[i]=='>') { col=40; ava=i+1; }
 }

 if (col==-1)
 {
  if (str[0]=='\1') col=41; // control line - fidonet
  else
  if (len>2 && strncmp(str,"...",3)==0) col=42; // tag line - fidonet
  else
  if (len>2 && strncmp(str,"---",3)==0) col=43; // tear line - fidonet
  else
  if (len>9 && strncmp(str," * Origin:",10)==0) col=44; // origin line - fidonet
 }

 if (col>-1) col=config.col[col];

 col<<=8;

 while(ava>0)
 {
  if (col>-1 && pos>=vidx && pos<vidx+config.maxx)
   buf[pos-vidx]=(buf[pos-vidx]&0xff)|col;
  pos++; ava--;
 }

 return(0);
}

/***************************************************************************/
/* scrive a video un elemento del testo
   se mcr=corr e il buffer c'e', prendo i dati dal buffer
   tengo conto del config.cole diverso in caso di blocchi vari
   se mcr==0 prendo il corrente
   se mcr==1 prendo il corrente senza pero' il config.cole di "corrente" */

void editor::putelm(elem *mcr,int nn,int y)
{
 static int updating=0;
 char *str;
 int l,ll;
 elem *current;
 short int col;
 short int vidbuf[MAXVIDX]; // prima costruisco il tutto in un bufferino

 current=corr;

 if (!mcr) { mcr=corr; nn=lcorr; } // se mcr nullo, scrivo corr
 if (mcr!=(elem *)1)
 {
  current=mcr;

  if (mcr!=corr)
  {
   str=(char *)irdati(mcr); ll=l=irlung(mcr);
   if (docmode && nn>=pari && nn<=pare) col=config.col[18];
                                      else col=config.col[1];
  }
  else
  {
   if (flag&FL_BUFFER)
   { str=buffer; ll=l=lbuf; if (binary) { ll-=59; l-=59; str+=59; } }
   else { str=(char *)irdati(mcr); ll=l=irlung(mcr); }
   col=config.col[2];
  }
 }
 else // mcr==1, scrivo corr non selezionato
 {
  str=rdati(); ll=l=rlung(); nn=lcorr;
  if (docmode && nn>=pari && nn<=pare) col=config.col[18];
   else col=config.col[1];
 }

 // str punta all'inizio del buffer, l e' la lunghezza del buffer
 // vidx e' il primo carattere da sbattere a video

 { // riempo il mio bufferino e sistemo gli attributi
  short int *start,col2;
  int i;
  unsigned char *str2;

  col2=(short int)(col<<(short int)8); start=vidbuf;

  if (config.shwtab && !binary)
  {
   int cmx=config.maxx;

   l=(l-vidx)>?0;
   str2=(unsigned char *)str+vidx;
   if (l>cmx) l=cmx;
   for (i=0; i<l; i++) *(start++)=col2|(short int)*(str2++);
   for (; i<config.maxx; i++) *(start++)=col2|(short int)' ';
  }
  else
  if (binary)
  {
   unsigned char bin[80];
   int cmx=config.maxx;

   memset(bin,' ',80);
   memcpy(bin+59,str,l);

   sprintf((char *)bin,"%08X",nn*16); bin[8]=' ';

   if (l==16)
   {
    sprintf((char *)bin+10,"%02X %02X %02X %02X %02X %02X %02X %02X "
             "%02X %02X %02X %02X %02X %02X %02X %02X",
    bin[59+0],bin[59+1],bin[59+2],bin[59+3],bin[59+4],bin[59+5],bin[59+6],
    bin[59+7],bin[59+8],bin[59+9],bin[59+10],bin[59+11],bin[59+12],bin[59+13],
    bin[59+14],bin[59+15]);

    bin[10+15*3+2]=' ';
   }
   else
   for (i=0; i<l; i++)
   {
    sprintf((char *)bin+10+i*3,"%02X",bin[59+i]); bin[10+i*3+2]=' ';
   }

   l+=59;

   l=(l-vidx)>?0;
   str2=(unsigned char *)bin+vidx;
   if (l>cmx) l=cmx;
   for (i=0; i<l; i++) *(start++)=col2|(short int)*(str2++);
   for (; i<config.maxx; i++) *(start++)=col2|(short int)' ';
  }
  else // espansione tab!
  { // TAB,tab...
    // l=lunghezza stringa da scrivere str=stringa da scrivere
    // x2=posizione a video t=spazi da inserire

   int t,x2;

   for (t=0,x2=0,str2=(unsigned char *)str; l>0 && x2<vidx; x2++)
    if (t) t--;
    else
    {
     if (*str2=='\t') { t=config.tabsize-(x2%config.tabsize)-1; } str2++; l--;
    }

   for (i=0; l>0 && i<config.maxx; i++,x2++)
   {
    if (t) { *(start++)=col2|(short int)' '; t--; }
    else
    {
     if (*str2=='\t')
     { t=config.tabsize-(x2%config.tabsize)-1; *(start++)=col2|(short int)' '; }
     else *(start++)=col2|(short int)(*str2);

     str2++; l--;
    }
   }
//   i+=vidx;
   for (; i<config.maxx; i++) *(start++)=col2|(short int)' ';
  }
 }

 if (colorize)
 {
  int newstate;

  switch(colorize)
  {
   case 2: newstate=coloracpp((unsigned short int *)vidbuf,str,vidx,ll,
                              icolret(current)); break; // C++ mode
   case 3: newstate=colorahtml((unsigned short int *)vidbuf,str,vidx,ll,
                               icolret(current)); break; // html mode
   case 4: newstate=coloratex((unsigned short int *)vidbuf,str,vidx,ll,
                               icolret(current)); break; // tex mode
   case 5: newstate=coloramail((unsigned short int *)vidbuf,str,vidx,ll
                               ); break; // mail mode
   case 6: newstate=colorajava((unsigned short int *)vidbuf,str,vidx,ll,
                              icolret(current)); break; // java mode
   default: newstate=0; break;
  }

  if (!updating && irnext(current) && icolret(irnext(current))!=newstate)
  { // AAAAGGGG ! devo aggiornare il resto del file !
   elem *scorr=irnext(current);
   int yy=y+1;
   int nnn=nn+1;

   updating=1;

   do
   {
    isetcol(scorr,newstate);

    switch(colorize)
    {
     case 2: newstate=coloracpp(0,irdati(scorr),0,irlung(scorr),
                                icolret(scorr)); break; // C++ mode
     case 3: newstate=colorahtml(0,irdati(scorr),0,irlung(scorr),
                                 icolret(scorr)); break; // html mode
     case 4: newstate=coloratex(0,irdati(scorr),0,irlung(scorr),
                                 icolret(scorr)); break; // html mode
     case 5: newstate=coloramail(0,irdati(scorr),0,irlung(scorr)
                                 ); break; // html mode
     case 6: newstate=colorajava(0,irdati(scorr),0,irlung(scorr),
                                icolret(scorr)); break; // java mode
     default: newstate=0; break;
    }

    if (yy<=ya) putelm(scorr,nnn,yy);

    scorr=irnext(scorr);
    if (scorr && icolret(scorr)==newstate) break;
    yy++; nnn++;
   } while(scorr);

   updating=0;
  }
 }

 int xs=0,xe=0;

 if (block.blk==7 && block.edtr==this)
 { // se blocco selezionato e l'editor corrisponde a questo, gestione blocco
  int s,e,il;
  int cs,ce,i;
  s=block.start; e=block.end; cs=block.cstart; ce=block.cend;
  if (s>e) { il=s; s=e; e=il; i=cs; cs=ce; ce=i; }

  if (block.type && cs>ce) { i=cs; cs=ce; ce=i; }
  if (s==e && cs>ce) { i=cs; cs=ce; ce=i; }
  if (cs==1023 && ce==0) { cs=0; ce=1023; }

  if (nn>=s && nn<=e)
  {
   if (block.type) { xs=cs; xe=ce; } // a colonna
   else
   {
    if (nn==s && nn==e) { xs=cs; xe=ce; }
    else if (nn==s) { xs=cs; xe=1023; }
    else if (nn>s && nn<e) { xs=0; xe=1023; }
    else if (nn==e) { xs=0; xe=ce; }
   }
   xs-=vidx; xe-=vidx;
   if (xs<0) xs=0;
   if (xe>MAXX) xe=MAXX;
   if (xs<MAXX && xe>0)
   { // devo colorare di "blocco" il pezzo di buffer da xs a xe
    short int *start;
    int fi;
    char col2;

    if (mcr!=corr)
    {
     if (docmode && nn>=pari && nn<=pare) col2=config.col[19];
                                        else col2=config.col[10];
    } else col2=config.col[13];
    start=vidbuf+xs; fi=xe-xs;

    int i;

 #ifndef UNIX
    for (i=0; i<fi; i++) *((char *)(start++)+1)=col2;
 #else
    for (i=0; i<fi; i++) { *start=((*start)&0xff)|(col2<<8); start++; }
 #endif

   }
  }
 }
 // copio tutto nel buffer principale

 if (config.ansi) astatok(y,y,0,config.maxx-1);
 if (config.ansi>0 && config.ansi<128) // scan per i codici di controllo
 {
  int i;

  for (i=0; i<config.maxx; i++)
  {
   int chto=0,ch;

   ch=((unsigned short int)vidbuf[i])&0xff;

   if (ch<' ') chto=ch+'a'-1;
   else
   if ((ch&0x7f)<' ') chto=(ch&0x7f)+'A'-1;
   else
   if (ch==0x7f) chto='$';
   if (chto==0x7f) chto='#';

   if (chto)
    vidbuf[i]=(short int)((short int)(((short int)config.col[16])<<((short int)8))
             |(short int)chto);
  }
 }
 {
  void *des;

  des=vbuf+config.maxx*y;
  l=config.maxx;
  memcpy(des,vidbuf,l*2);
 }
}

/***************************************************************************/
// converte video x in real x
// la uso in vari comandini

int editor::vtor(int r,elem *elm)
{ // vuoi i tab ? cuccati tutta 'sta roba in piu'
 if (!config.shwtab)
 {
  char *cb;
  int lb,i,t;

  if (elm==0)
  {
   if (flag&FL_BUFFER) { cb=buffer; lb=lbuf; }
                  else { cb=(char *)rdati(); lb=rlung(); }
  }
  else { cb=(char *)irdati(elm); lb=irlung(elm); }
  for (t=i=0; i<lb && t<r; i++,cb++)
   if (*cb=='\t') t+=(config.tabsize-(t%config.tabsize)); else t++;
  if (t>r) r=i; else r=i+r-t;
 }
 return(r);
}

/***************************************************************************/
// la routine elimina TAB - da eseguire a vista di un tab
// all'interno del buffer

int editor::tabexp(char *start,short int *l)
{
 char *p;
 int mod=0;

 while ((p=(char *)memchr((void *)start,'\t',*l))!=0)
 {
  int i,t;

  i=p-start; t=config.tabsize-(i%config.tabsize);

  *p=' '; t--; mod++;

  if (t+*l>1023) *l=1024-t;
  if (t) { memmove(p+t,p,*l-i); memset(p,' ',t); *l+=t; }
 }
 return(mod);
}

/***************************************************************************/
// converte real x in video x
// la uso in vari comandini

int editor::rtov(int v)
{
 int vv;

 if (!config.shwtab)
 { // vuoi i tab ? cuccati questo ciclo in piu' !
  char *cb;
  int lb,i,t;

  if (flag&FL_BUFFER) { cb=buffer; lb=lbuf; }
                 else { cb=(char *)rdati(); lb=rlung(); }
  for (t=i=0; i<lb && i<v; i++,cb++)
   if (*cb=='\t') t+=(config.tabsize-(t%config.tabsize)); else t++;
  vv=t+v-i;
 } else vv=v;
 return(vv);
}

/***************************************************************************/
// posiziona il cursore, se questo e' posizionato fuori dallo schermo,
// eseguo uno scroll orizzontale

void editor::setcur(void )
{
 vx=rtov(x);

 if (!(vx>=vidx && vx<(vidx+MAXX)))
 {
  if (vx>(vidx+MAXX-1)) vidx=(vx-MAXX+COL_TO_SCROLL)<?(MAXLINE-MAXX);
  else if (vx<vidx) vidx=0>?(vx-COL_TO_SCROLL);
  putpag();
 }
 vsetcur(vx-vidx,vidy); //if (!todraw) puthdr(3);
}

/***************************************************************************/
/*
 scrive la testata della finestra
 cosa = riscrivo ....
 1 = "linea"  2 = "colonna"  4 = "flag"  8 = "mem libera" 16 = "nome file"
*/

void editor::puthdr(int cosa)
{
 if ((cosa & 3) && todraw!=2 && !(flag&FL_SCRITT))
 {
  putint(vx+1,   5, 0,yda,config.col[0]);
  putint(lcorr+1,7, 5,yda,config.col[0]);
  putint(rtot(), 7,12,yda,config.col[0]);
 }
 if (cosa & 4)
 {
  putchn(' ',12,19,yda,config.col[0]);
  if (docmode) putchr('D',19,yda,config.col[0]);
  if (flag & FL_NEWFILE) putchr('N',20,yda,config.col[0]);
  if (flag & FL_MODIF  ) putchr('M',22,yda,config.col[0]);
  if (!config.shwtab) putchr('T',23,yda,config.col[0]);
  if (readonly) putchr('R',24,yda,config.col[0]);
   else if (flag & FL_SOVR) putchr('S',24,yda,config.col[0]);
   else putchr('I',24,yda,config.col[0]);
  if (config.indent) putchr('A',25,yda,config.col[0]);
  if (flag & FL_BCOL) putchr('K',26,yda,config.col[0]);
  if (flag & FL_BLDR) putchr('L',27,yda,config.col[0]);
  if (flag & FL_BCDR) putchr('C',27,yda,config.col[0]);
  if (config.showmatch) putchr('W',28,yda,config.col[0]);
  if (macrocmd) putchr('R',29,yda,config.col[0]);
 }
 #ifndef UNIX
  if (cosa & 8)
  {
   char str[20];
   char ch;
   unsigned int cl;

   cl=_go32_dpmi_remaining_virtual_memory();
   cl/=1024; ch='K';
   if (cl>999) { cl/=1024; ch='M'; }
   if (cl>999) { cl/=1024; ch='G'; }
   sprintf(str,"%3i",(int)cl);
   putstr(str,3,31,yda,config.col[0]);
   putchr(ch,34,yda,config.col[0]);
  }
 #else
  if (cosa & 8) putchn(' ',5,31,yda,config.col[0]);
 #endif
 if (cosa & 16)
 {
  char path[128],*st;
  int l;

  putstr(" Zed      ",10,35,yda,config.col[0]);
  putstr(retstvers(),4,40,yda,config.col[0]);

  if (clip.edtr==this) strcpy(path,"<CLIPBOARD>");
  else
  if (delbuf==this) strcpy(path,"<DELETE BUFFER>");
  else
  if (filename[0]=='>' && filename[1]=='H') strcpy(path,"<HELP>");
  else strcpy(path,filename);

  l=strlen(path);
  if (l>MAXX-45) { st=&path[l-(MAXX-45)]; *st='.'; *(st+1)='.'; }
  else st=path;
  putstr(st,0,45,yda,config.col[0]);
  l=strlen(st);
  putchn(' ',(MAXX-45-l),l+45,yda,config.col[0]);
 }
}

/***************************************************************************/
// (ri)scrive a video il "resto" (dalla riga corrente in giu').

void editor::putres(int y,elem *scorr,int nn)
{
 do
 {
  putelm(scorr,nn,y); if (scorr==corr) vidy=y;
  y++; scorr=irnext(scorr); nn++;
 } while (y<=ya && scorr!=0);
 if (scorr==0 && y<=ya)
 {
  vputstr(EOF_STR,EOF_STR_LEN,y,config.col[3]);
  y++; while (y<=ya) putchn(' ',MAXX,0,y++,config.col[1]);
 }
}

/***************************************************************************/
// riscrive l'intera pagina, usata in caso di grandi modifiche (es. blocchi)

void editor::putpag(void )
{
 int y; // row to scroll calcolati
 elem *scorr;
 int nn;

 if (vidy>ya) vidy=yda+(ya-yda)/2;
 y=vidy; scorr=corr; nn=lcorr;
 while (irprev(scorr) && y>yda+1) { scorr=irprev(scorr); y--; nn--; }

 putres(yda+1,scorr,nn);
}

/***************************************************************************/

void editor::putdaa(int rdda,int rda)
{
 if (rdda>rda) { int t=rdda; rdda=rda; rda=t; }
 rdda=rdda>?(lcorr-((int)((vidy-yda)-1)));
 rda=rda<?(lcorr+(int)(ya-vidy));
 if (rda>rtot()-1) rda=rtot()-1;

 if (rda>=rdda)
 {
  int y;
  elem *scorr;
  int nn;

  y=vidy; scorr=corr; nn=lcorr;
  while (irprev(scorr) && nn>rdda) { scorr=irprev(scorr); y--; nn--; }
  while (irnext(scorr) && nn<rdda) { scorr=irnext(scorr); y++; nn++; }

  do
  {
   putelm(scorr,nn,y); if (scorr==corr) vidy=y;
   y++; scorr=irnext(scorr); nn++;
  } while (nn<=rda && scorr!=0);
  if (scorr==0 && y<=ya)
  {
   vputstr(EOF_STR,EOF_STR_LEN,y,config.col[3]);
   y++; while (y<=ya) putchn(' ',MAXX,0,y++,config.col[1]);
  }
 }
}

/***************************************************************************/

int editor::isetcol(elem *mio,int stato)
{
 int old=mio->lung&0x1c00;
 int mnew=(stato&0x7)<<10;

 mio->lung=(mio->lung&0xe3ff)|mnew;
 if (old!=mnew) return(1); else return(0);
}

/***************************************************************************/

int editor::setcol(int stato)
{
 int old=corr->lung&0x1c00;
 int mnew=(stato&0x7)<<10;

 corr->lung=(corr->lung&0xe3ff)|mnew;
 if (old!=mnew) return(1); else return(0);
}

/***************************************************************************/
// riscrivo l'editor su video con le dimensioni date

void editor::getup(int yda1,int ya1)
{
 if (!corr) add("",0,3); // non controllo la memoria - e' un sanity check
 if (vidy<(yda+1) || vidy>ya) vidy=(ya1-(yda1+1))/2;
 yda=yda1; ya=ya1;

 if (flag&FL_SCRITT) puthdr(252); else puthdr(255);
 flag&=~FL_SCRITT;
 todraw=2; putpag(); setcur();
 if (docmode) execcmd(96); // per scrivere l'eventuale paragrafo
 execcmd(104);
 todraw=1;
}

/***************************************************************************/
// controllo se devo ripassare il file per la colorizzazione
// se type=0 in caso di controllo mi fermo al primo giusto
// se type=-1 in caso di controllo passo tutto il file
// se type>1 in caso di controllo passo type righe, poi mi fermo al primo giusto
// se passo le stringhe controllo se ci sono caratteri speciali
//
// ret=0 fatto niente else numero di righe modificate(1 sempre)

int editor::colcheck(elem *scorr,int type,char *str1,char *str2)
{
 if (!colorize || !scorr) return(0);
 if (str1 || str2)
 {
  int todo=0;
  char *special="#/*\"\\\'<$"; // caratteri che possono cambiare lo stato
  int i;

  for (i=0; special[i]!='\0' && todo==0; i++)
  {
   if (str1 && strchr(str1,special[i])) todo=1;
   if (str2 && strchr(str2,special[i])) todo=1;
  }
  if (!todo) return(0);
 }

 char *save=0;

 if (irprev(scorr)==0 && type==-1 && yda>-1)
 {
  save=new char [config.maxx*2+2];
  saveline(yda,save);

  putstr("Setting up colors...     ",25,0,yda,config.col[6]);
  okstatus(yda);
 }

 int newstate=0,ret=0;

 if (irprev(scorr))
 {
  scorr=irprev(scorr);
  if (type==0) type=1; if (type>0) type++;
 }

 do
 {
  if (ret) isetcol(scorr,newstate);

  switch(colorize)
  {
   case 2: newstate=coloracpp(0,irdati(scorr),0,irlung(scorr),
                              icolret(scorr)); break; // C++
   case 3: newstate=colorahtml(0,irdati(scorr),0,irlung(scorr),
                               icolret(scorr)); break; // html
   case 4: newstate=coloratex(0,irdati(scorr),0,irlung(scorr),
                              icolret(scorr)); break; // tex
   case 5: newstate=coloramail(0,irdati(scorr),0,irlung(scorr)
                               ); break; // mail mode
   case 6: newstate=colorajava(0,irdati(scorr),0,irlung(scorr),
                              icolret(scorr)); break; // Java
   default: newstate=0; break;
  }

  scorr=irnext(scorr);
  if (scorr && type==0 && icolret(scorr)==newstate) break;
  if (type>0) type--;
  ret++;
 } while(scorr);

 if (save)
 {
  restline(yda,save);
  delete save;
 }

 return(ret+1);
}

/***************************************************************************/

