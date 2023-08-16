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

/* config.cc - compilatore/gestore della configurazione */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <regex.h>
#ifdef X11
 #include <X11/Xlib.h>
#endif

#ifdef _AIX
 #include <strings.h>
#endif

#include "zed.h"

#ifndef UNIX
 #include <dos.h>
#else
 #include <sys/ioctl.h>
#endif

/***************************************************************************/

struct toks
{
 int pos;
 char *st;
};

int loading=0;

/***************************************************************************/

_config config;
char *configs=0;
int pwdln=30;
char *s_oom="Out Of Memory, hit ESC";
short int *vbuf=0;
short int *avideo=0;
_modif *astat=0;
_block block;
_clip clip={0,0,0,0,0};
_match match={0,0,-1,0,0};
editor *delbuf;

static struct
{
 char version[10]; // versione scritta nella prima pagina
 char stvers[10];  // versione scritta nella riga header
 char testata[80]; // testata
} maindata={"V 1.0.5","1.05","Zed V1.0.5 by Sandro Serafini (c) 1997/98\n"};

/***************************************************************************/

char *mstrtok(toks *conf,char *s1,unsigned int *xkeys=0);
int riga(int file,char *buff,char *ori,mfile *mf);

/***************************************************************************/

// isstr= 0=int 1=char* 2=XColor

static struct // struttura dei parametri
{
 char *par;
 char isstr;
 int *parint;
} tabpar[]={{"nospace",0,&config.nospace},
 {"columns",0,&config.maxx},{"rows",0,&config.maxy},
 {"maxdelbuf",0,&config.maxdelbuf},
 {"colormode",0,&config.colormode},
 {"coltoscroll",0,&config.coltoscroll},
 {"colstatus",0,&config.col[0]},{"coltext",0,&config.col[1]},
 {"colcurr",0,&config.col[2]},{"coleof",0,&config.col[3]},
 {"coldialog",0,&config.col[4]},{"colerror",0,&config.col[5]},
 {"colwarning",0,&config.col[6]},{"colsel",0,&config.col[7]},
 {"colcurrsel",0,&config.col[8]},{"colselect",0,&config.col[9]},
 {"colblock",0,&config.col[10]},
 {"colfndtext",0,&config.col[12]},{"colcurrblk",0,&config.col[13]},
 {"showmatch",0,&config.showmatch},{"colmatch",0,&config.col[14]},

 {"colcomment",0,&config.col[20]},{"colpreprocessor",0,&config.col[21]},
 {"colsymbol",0,&config.col[22]},{"colidentifier",0,&config.col[23]},
 {"coldecimal",0,&config.col[24]},{"colesadecimal",0,&config.col[25]},
 {"coloctal",0,&config.col[26]},{"colfloat",0,&config.col[27]},
 {"colstring",0,&config.col[28]},{"colchar",0,&config.col[29]},
 {"colerror",0,&config.col[30]},{"colreserved",0,&config.col[31]},

 {"colhtmlcmd",0,&config.col[32]},{"colhtmlpar",0,&config.col[33]},
 {"colhtmlcon",0,&config.col[34]},

 {"coltexcomm",0,&config.col[35]},{"coltexmath",0,&config.col[36]},
 {"coltexcmd",0,&config.col[37]},

 {"colmailqt",0,&config.col[40]},{"colmailctl",0,&config.col[41]},
 {"colmailtag",0,&config.col[42]},{"colmailtear",0,&config.col[43]},
 {"colmailori",0,&config.col[44]},

 {"streof",1,&config.streof},
 {"streop",1,&config.streop},{"strsop",1,&config.strsop},
 {"streol",1,&config.streol},
 {"strfnddef",1,&config.strfnddef},{"strrpldef",1,&config.strrpldef},
 {"strifnddef",1,&config.strifnddef},
 {"strmail",1,&config.strmail},
 {"ansi",0,&config.ansi},{"colcode",0,&config.col[16]},
 {"colparam",0,&config.col[17]},{"indent",0,&config.indent},
 {"showtab",0,&config.shwtab},{"tabexpandl",0,&config.tabexpl},
 {"tabexpandr",0,&config.tabexpr},{"tabsize",0,&config.tabsize},
 {"indcfg",0,&config.indcfg},
 {"rmargin",0,&config.rmargin},
 {"lmargin",0,&config.lmargin},{"jindent",0,&config.rient},{"doc",0,&config.doc},
 {"colpara",0,&config.col[18]},{"colbpara",0,&config.col[19]},
 {"softret",0,&config.softret},{"wholeline",0,&config.whole},
 {"bak",0,&config.bak},{"usrvar",0,&config.vars},
 {"autosave",0,&config.autosave},{"wordwrap",0,&config.ww},
#ifdef UNIX
 {"delay",0,&config.delay},{"shiftlinux",0,&config.shiftlinux},
#endif
#ifdef X11
 {"display",1,&config.display},{"geometry",1,&config.geometry},
 {"font",1,&config.font},
                              {"pal1",1,&config.xcol[ 1]},
 {"pal2",1,&config.xcol[ 2]}, {"pal3",1,&config.xcol[ 3]},
 {"pal4",1,&config.xcol[ 4]}, {"pal5",1,&config.xcol[ 5]},
 {"pal6",1,&config.xcol[ 6]}, {"pal7",1,&config.xcol[ 7]},
 {"pal8",1,&config.xcol[ 8]}, {"pal9",1,&config.xcol[ 9]},
 {"pala",1,&config.xcol[10]}, {"palb",1,&config.xcol[11]},
 {"palc",1,&config.xcol[12]}, {"pald",1,&config.xcol[13]},
 {"pale",1,&config.xcol[14]},
#endif
 {(char *)0,(char)0,(int *)0}};

commands tabcom[]={{"c_down",1},{"c_up",2},{"p_down",3},{"p_up",4},
 {"g_sof",5},{"g_eof",6},{"g_sos",7},{"g_eos",8},{"e_canc",130},
 {"c_right",131},{"c_left",132},{"g_eol",133},{"g_sol",134},
 {"e_bkspc",135},{"e_enter",136},{"e_delline",40},{"l_instog",138},
 {"l_insset",139},{"f_savefile",9},{"w_quit",10},{"w_split",11},
 {"e_putcode",140},{"e_escape",12},{"w_qquit",13},{"f_select",112},
 {"g_nwr",142},{"g_pwr",143},{"f_killtab",113},
 {"w_quita",14},{"w_qquita",15},{"f_saveall",16},{"f_savemodif",103},
 {"w_winres",17},{"w_winnext",18},{"w_winprev",19},
 {"f_fileld",20},{"w_bufnext",21},{"w_bufprev",22},
 {"f_filesb",23},{"w_zoom",24},{"w_zoomq",25},
 {"w_bufsel",26},{"e_inscode",141},{"e_hexcode",191},
 {"b_toggle",144},{"b_begin",145},{"b_end",146},{"b_ldrag",147},
 {"b_cdrag",148},{"b_cresize",150},{"b_hide",151},{"b_fhide",156},
 {"b_setpnt",152},{"b_setnorm",153},{"b_setnold",154},{"b_setnocd",155},
 {"b_clpdel",27},{"b_clpcut",30},
 {"b_clpcopy",31},{"b_clppaste",32},{"b_blockdel",33},{"b_clppstd",34},
 {"f_chname",35},{"b_clpsave",36},{"b_clpload",37},{"e_entwait",100},
 {"b_deldelb",38},{"b_resdelb",39},{"e_deleol",161},{"s_isearch",41},
 {"s_sagain",42},{"s_search",43},{"s_replace",44},
 {"l_swmtog",162},{"l_swmres",163},{"w_redrawall",47},
 {"f_cconfig",48},{"g_sob",49},{"g_eob",50},{"g_c2sos",51},{"g_c2eos",52},
 {"g_c2mos",53},{"g_sup",54},{"g_sdown",55},
 {"g_line",58},{"l_idntog",164},{"l_idnset",165},{"b_unsel",166},
 {"e_tolower",167},{"e_toupper",168},{"e_flipcase",169},{"f_shell",170},
 {"w_hide",171},{"l_shwtabt",172},{"l_shwtabs",173},
 {"e_strecmcr",60},{"e_stmacro",61},{"e_abmacro",62},{"e_recmacro",63},
 {"g_set0",64},{"g_set1",65},{"g_set2",66},{"g_set3",67},
 {"g_go0",68},{"g_go1",69},{"g_go2",70},{"g_go3",71},
 {"j_rmargin",72},{"j_lmargin",73},{"j_indent",74},{"b_indent",105},
 {"l_doctog",174},{"l_docset",175},{"j_soft",75},{"j_hard",79},
 {"j_block2par",115},{"j_quoterem",116},{"j_quoteres",117},
 {"e_senter",176},{"j_center",177},{"j_right",178},{"j_left",179},
 {"e_deind",180},{"j_format",76},{"j_justif",77},{"j_dejust",78},
 {"j_allhard",80},{"e_wrmacro",81},{"g_parent",82},{"b_fill",83},
 {"b_toupper",84},{"b_tolower",85},{"b_flipcase",86},
 {"b_shl",87},{"b_shr",88},{"g_nextpar",89},{"g_prevpar",90},
 {"e_openall",91},{"e_hexopen",114},{"e_roopen",118},
 {"f_qreload",92},{"f_reload",93},{"b_clphere",94},
 {"b_clpzero",95},{"b_ishl",97},{"b_ishr",98},{"g_function",99},
 {"e_restore",181},{"g_lmargin",182},{"w_winsave",101},{"w_wingoto",102},
 {"b_rot13",119},
 {"l_seta",183},{"l_setb",184},{"l_setc",185},{"l_setd",186},
 {"l_toga",187},{"l_togb",188},{"l_togc",189},{"l_togd",190},{"g_menu",192},
 {"b_clpexport",106},{"b_clpexportd",107},{"b_clpexportl",108},
 {"b_clpexportdl",109},{"b_clpimport",110},{"l_autosave",193},
 {"m_gotowin",194},{"m_gotozed",195},{"l_noop",196},{"f_aconfig",111},
 {0,255}};

/***************************************************************************/

void outtestata(void)
{
 output(maindata.testata);
}

/***************************************************************************/

int htoi(char ch)
{
 return((isdigit(ch))?ch-'0':tolower(ch)-'a'+0xa);
}

/***************************************************************************/

int riga(char *buff,char *ori,int *error,mfile *mf)
{
 int len,eofm,i;

 if ((eofm=mread(&buff,198,error,&len,mf))==0)
 {
  len&=0xfff;
  buff[len]='\0';

  // tolgo spazi e tab iniziali
  while(buff[0]==' ' || buff[0]==9) strcpy(buff,buff+1);
  // tolgo spazi, tab e 0x10 dalla fine
  for (i=strlen(buff)-1; (buff[i]==' ' || buff[i]==9 || buff[i]==10) && i>0;
   i++) buff[i]='\0';
  strcpy(ori,buff);
  // tolgo i tab ed i commenti
  for (i=0; buff[i]!='\0'; i++)
  {
   if (buff[i]==9) buff[i]=' '; // tolgo i tab
   if (buff[i]==';') // tolgo i rem o i doppi ;;
    if (i>0 && buff[i-1]=='\\') strcpy(&buff[i-1],&buff[i]);
    else { buff[i]='\0'; break; }
  }
 }
 return(eofm);
}

/***************************************************************************/
// 0 ok
// -1 errore nel command line

int parsecm(int argc,char *argv[],char *nome)
{
 int argr=1;
 char str[8192];
 int pstr=0;

 if (argc)
 {
  char *p3;

  while(argc>argr && (argv[argr][0]=='-' || argv[argr][0]=='+'))
  {
   if (stricmp(argv[argr],"--")==0) { argr++; break; }
   else
   if (argv[argr][0]=='+')
   {
    config.gotoline=strtol(argv[argr]+1,&p3,0);
    argr++;
   }
   else
   if (stricmp(argv[argr],"-def")==0)
   {
    config.config=1;
    argr++;
   }
   else
   if (stricmp(argv[argr],"-cfg")==0)
   {
    if (argc<=argr+1) return(-1);
    strcpy(nome,argv[argr+1]);
    argr+=2;
   }
   else
   if (stricmp(argv[argr],"-add")==0)
   {
    if (argc<=argr+1) return(-1);
    if (cconfig(argv[argr+1])) return(-1);
    argr+=2;
   }
   else
   {
    if (argc==argr+1) return(-1); // ne servono almeno due (comando, parametro)
    else
    {
     int v=0;
     int tok;

     for (tok=0; (tabpar[tok].parint!=0) &&
                 (stricmp(argv[argr]+1,tabpar[tok].par)); tok++);
     if (tabpar[tok].parint==0) return(-1);

     if (tabpar[tok].isstr)
     { // stringa
      *tabpar[tok].parint=pstr; strcpy(str+pstr,argv[argr+1]);
      pstr+=strlen(argv[argr+1])+1;
     }
     else
     { // numerico
      if (!stricmp(argv[argr+1],"on")) v=1;
      else if (!stricmp(argv[argr+1],"off")) v=0;
      else v=(short int)strtol(argv[argr+1],&p3,0);
      *tabpar[tok].parint=v;
     }
    }
    argr+=2;
   }
  }
 }
 if (pstr>0)
 {
  config.strs=new char[pstr]; memcpy(config.strs,str,pstr);
  config.pstrs=pstr;
 }
 return(argr);
}

/***************************************************************************/
// myname oppure quale==0 compilo il locale, ==1 compilo il default,
// ==2 compilo interno(non implementato)
// ==3 controllo solo se c'e' o no il locale

// ret:
// 0 == ok
// -2 errore nel file di configurazione

int lconfig(int quale,char *myname)
{
 int err=0;
 char nome[512];

 // eventuale azzeramento _prima_ di entrare qui !

 loading=0; delete configs; configs=0;

 // cerco il nome del file da caricare

 if (myname) strcpy(nome,myname);
 else
 { // if quale==0 or quale==1
  #ifdef MSDOS
   char *p1;

   strcpy(nome,argv0); p1=strrchr(nome,'.');
   if (!p1)
   {
    output("\nNo startup pathname info (no argv[0])\n");
    strcpy(nome,"ZED.EXE"); p1=strrchr(nome,'.');
   }
   *(++p1)='\0'; // tronco dopo il "."
   strcat(nome,"cfg");
  #else
   char *h=getenv("HOME");
   if (h==0) { output("\nPlease set/export $HOME !\n"); err=1; }
   if (err==0)
   {
    strcpy(nome,h);
    #ifdef X11
     strcat(nome,"/.zedxrc");
    #else
     strcat(nome,"/.zedrc");
    #endif
   }
  #endif

  if (quale==1 && err==0)
  {
   #ifdef MSDOS
    *p1='\0'; // ritronco dopo il "."
    strcat(nome,"std");
   #else
    #ifdef X11
     if (access("/etc/zedxrc",R_OK)==0)
      strcpy(nome,"/etc/zedxrc"); else strcpy(nome,"/usr/local/etc/zedxrc");
    #else
     if (access("/etc/zedrc",R_OK)==0)
      strcpy(nome,"/etc/zedrc"); else strcpy(nome,"/usr/local/etc/zedrc");
    #endif
   #endif
  }
 }

 if (err==0)
 {
  if (quale==3) err=access(nome,R_OK); else err=cconfig(nome);

 #if defined __linux && !defined X11
  if (err==0)
  {
   int s=6;

   if (config.shiftlinux!=0 && config.shiftlinux!=-1 &&
       ioctl(0,TIOCLINUX,&s)!=0)
   {
    output("Requested shift-status via TIOCLINUX not available !\n");
    output("Please use another configuration or zedx !\n");
    err=-2;
   }
  }
 #endif
 }

 if (err) return(-2);
 return(0);
}

/***************************************************************************/

void errore(char *nome,int l,char *str,char *ori)
{
 output(nome); output(" : "); outint(l); output(str); output(ori); output("\n");
}

/***************************************************************************/

int cconfig(char *nome)
{
 _config savecfg;
 mfile cfg;
 unsigned int xkeys[MAXXKEYS];
 char buff[200],ori[200],*str;
 unsigned char *cmd;
 unsigned int *key;
 _menu *mnu;
 int keylen,cmdlen,strlun,mnulen;
 int line,err,ifcount;
 int pcmd,pkey,pstr,pmnu;
 unsigned int **res;
 int pres,lres; // indirizzi da rilocare in str
 toks mytok;
 int menulvl=0;

/*
0=ok, 1=nomem, 2=open error, 10=read error,3=output open error,4=write error
5=file syntax error, 6=compilato troppo grande
*/

 if (loading==0) output("Loading "); else output("Reading ");
 output(nome); output("...\n");
 loading=1;

 memcpy(&savecfg,&config,sizeof(_config)); // salvo la configurazione

 err=0; // flag per la pausa finale

 pcmd=0; cmdlen=10240; cmd=new unsigned char[cmdlen];
 pkey=0; keylen=10240/4; key=new unsigned int[keylen];
 pstr=0; strlun=10240; str=new char[strlun];
 pmnu=0; mnulen=512; mnu=new _menu[mnulen];

 lres=1024; pres=0; res=new unsigned int *[lres];

 {
  if (err==0)
  {
   line=0;
   switch(mopen(nome,&cfg))
   {
    case 1 :
    case 2 : err=2; break;
    case 3 : err=1; break;
   }
  }

  line=0; ifcount=0;

  while(!riga(buff,ori,&err,&cfg) && err==0)
  {
   char *p2;

   line++; p2=mstrtok(&mytok,buff);

   if (p2)
   {
    if (!stricmp(p2,"include"))
    {
     if (menulvl)
     {
      errore(nome,line," : cannot include inside submenu definition : ",ori);
      err=5;
     }
     else
     if (err==0)
     {
      err=cconfig(mstrtok(&mytok,(char *)1,0));
      if (err) err-=100;
      output("Resuming "); output(nome); output("...\n");
     }
    }
    else
    if (!stricmp(p2,"print"))
    {
     output(mstrtok(&mytok,(char *)1,0)); output("\n");
    }
    else
    if (!stricmp(p2,"pause"))
    {
     output("\n\rPress ENTER to continue."); gettst3();
    }
    else
    if (!stricmp(p2,"error"))
    {
     outint(line); output(" : user error : "); output(mstrtok(&mytok,(char *)1,0)); output("\n");

     err=5;
    }
    else
    if (!stricmp(p2,"ifterm"))
    {
     ifcount++;

     int ok;
     char *p,*tm=getenv("TERM");

     if (tm==0) { output("\n\rPlease set/export $TERM !\n"); err=5; }
     else
     {
      ok=0;
      while ((p=mstrtok(&mytok,0))!=0) if (stricmp(tm,p)==0) ok=1;
      if (ok==0)
      {
       int iflvl=ifcount;
       int ok=1;
       do
       {
        ok=!riga(buff,ori,&err,&cfg); p=mstrtok(&mytok,buff); line++;
        if (ok)
        {
         if (stricmp(p,"ifterm")==0) ifcount++;
         if (stricmp(p,"endif")==0 && ifcount>iflvl) ifcount--;
         else
         if (stricmp(p,"endif")==0 && ifcount==iflvl) { ifcount--; iflvl--; }
        }
        else
        {
         output("\n\runexpected EOF without endif\n"); err=5;
        }
       } while(ok &&
        (ifcount!=iflvl  || (ifcount==iflvl && (stricmp(p,"else")!=0 && stricmp(p,"endif")!=0)))
        );
      }
     }
    }
    else
    if (!stricmp(p2,"else"))
    {
     if (ifcount==0) { errore(nome,line," : unexpected else : ",ori); err=5; }
    }
    else
    if (!stricmp(p2,"endif"))
    {
     if (ifcount==0) { errore(nome,line," : unexpected endif : ",ori); err=5; }
     else ifcount--;
    }
    else
    if (!stricmp(p2,"set"))
    { // e' un comando "set"
     char *p,*p1;

     p=mstrtok(&mytok,0); p1=mstrtok(&mytok,0);
     if (!p || !p1) { errore(nome,line," : Syntax error : ",ori); err=5; }
     else
     {
      int tok;

      for (tok=0; (tabpar[tok].parint!=0) && (stricmp(p,tabpar[tok].par)); tok++);
      if (tabpar[tok].parint==0) { errore(nome,line," : Unknown set : ",ori); err=5; }
      else
      {
       switch(tabpar[tok].isstr)
       {
        case 0: // numerico
        {
         int v=0;
         char *p3;

         if (!stricmp(p1,"on")) v=1;
         else if (!stricmp(p1,"off")) v=0;
         else v=(short int)strtol(p1,&p3,0);
         if (*tabpar[tok].parint==-1) *tabpar[tok].parint=v;
        } break;
        case 1 : // se e' stringa
        {
         if (*p1==34) p1++;
         if (*tabpar[tok].parint==-1)
         {
          *tabpar[tok].parint=pstr;
          res[pres++]=(unsigned int *)tabpar[tok].parint;
          strcpy(str+pstr,p1); pstr+=strlen(p1)+1;
         }
        } break;
       }
      }
     }
    }
    else
    if (!stricmp(p2,"key"))
    { // e' un comando "key..."
     int ierr;
     char *p;

     p=mstrtok(&mytok,0,xkeys);
     if (p==0)
     {
      ierr=0;
      if (xkeys[0]==0xffffffff)
      { errore(nome,line," : unknown shift status : ",ori); err=5; ierr=1; }

      { // if tasto ok - faccio lo stesso per controllare il resto
       if (ierr==0)
       { // verifico la sequenza con key
        int chk=0;      // parte la verifica
        unsigned int *kchk=key;

        while (chk<pkey && ierr==0)
        {
         int d=0; // d=1 se diversi

         d=(memcmp(&kchk[3],&xkeys[3],kchk[0]*4)!=0);

         if (d==0)
         {
          if (xkeys[0]==kchk[0] &&
              (((xkeys[1]&(~64))==0 && (kchk[1]&(~64))!=0) ||
               ((xkeys[1]&(~64))!=0 && (kchk[1]&(~64))==0))) ierr=4;
          else
          if ((xkeys[1]&(~64))==(kchk[1]&(~64)) && (kchk[0]==xkeys[0])) ierr=3;
          else
          if (!(kchk[1]&64) && (kchk[0]<xkeys[0]) && kchk[1]!=xkeys[1]) ierr=5;
          else
          if ((xkeys[1]&64) && (kchk[0]>xkeys[0])) ierr=6;
         }
         chk+=kchk[0]+3; kchk+=kchk[0]+3; // 3==len+shst+cmdoff
        }
       }

       if (ierr==0 && config.keys!=(unsigned int *)0xffffffff && config.keys!=0)
       { // verifico la sequenza con config.keys
        int chk=0;      // parte la verifica
        unsigned int *kchk=config.keys;

        while (chk<config.pkeys-2 && ierr==0)
        {
         int d=0; // d=1 se diversi

         d=(memcmp(&kchk[3],&xkeys[3],kchk[0]*4)!=0);

         if (d==0)
         {
          if (xkeys[0]==kchk[0] &&
              (((xkeys[1]&(~64))==0 && (kchk[1]&(~64))!=0) ||
               ((xkeys[1]&(~64))!=0 && (kchk[1]&(~64))==0))) ierr=4;
          else
          if ((xkeys[1]&(~64))==(kchk[1]&(~64)) && (kchk[0]==xkeys[0])) ierr=3;
          else
          if (!(kchk[1]&64) && (kchk[0]<xkeys[0]) && kchk[1]!=xkeys[1]) ierr=5;
          else
          if ((xkeys[1]&64) && (kchk[0]>xkeys[0])) ierr=6;
         }
         chk+=kchk[0]+3; kchk+=kchk[0]+3; // 3==len+shst+cmdoff
        }
       }
       if (ierr)
       {
        switch(ierr)
        {
         case 3:
          errore(nome,line," : key sequence already used : ",ori); err=5; break;
         case 4:
          errore(nome,line," : conflicting shift status : ",ori); err=5; break;
         case 5:
          errore(nome,line," : key sequence already used too short : ",ori); err=5; break;
         case 6:
          errore(nome,line," : short timed sequence goes first : ",ori); err=5; break;
        }
       }
       if (!ierr)
       {
        if (pkey+((int)xkeys[0]+10)*4>keylen)
        {
         keylen+=10240/4;
         key=(unsigned int *)realloc(key,keylen*4);
        }
        xkeys[2]=pcmd;
        memcpy(&key[pkey],xkeys,(xkeys[0]+3)*4);
        pkey+=xkeys[0]+3;
       }
       // adesso devo mettere i comandi da eseguire
       while ((p=mstrtok(&mytok,0))!=0)
       {
        if (!stricmp(p,"\\"))
        { riga(buff,ori,&err,&cfg); p=mstrtok(&mytok,buff); line++; }
        if (*p!=34)
        {
         int tok;

         for (tok=0; tabcom[tok].ncom<255; tok++)
         {
          if (!stricmp(p,tabcom[tok].com))
          {
           if (ierr==0) { cmd[pcmd]=tabcom[tok].ncom; pcmd++; }
           break;
          }
         }
         if (err==0 && tabcom[tok].ncom==255)
         {
          errore(nome,line," : Unknown function : ",ori); err=5;
         }
        }
        else
        { // e' una stringa, la trasloco
         p++;

         if (ierr==0)
         {
          cmd[pcmd]=0xff; // token per "stringa"
          cmd[pcmd+1]=strlen(p); pcmd+=2;
          strcpy((char *)(cmd+pcmd),p); pcmd+=strlen(p);
         }
        } // fine gestione stringa

        if (pcmd+1024>cmdlen)
        {
         cmdlen+=10240;
         cmd=(unsigned char *)realloc(cmd,cmdlen);
        }

       } // fine ciclo input comandi
       if (ierr==0) { cmd[pcmd]=0; pcmd++; } // tappo
      } // fine if sequenza di tasti OK
     } // if token ok
    } // fine gestione input descrizione tasto/comando
    else
    if (!stricmp(p2,"menu")) // -2 vuoto/impossibile -1 da riempire
    {
     menulvl++;
     char *p;

     p=mstrtok(&mytok,0);
     if (!p) { errore(nome,line," : Syntax error : ",ori); err=5; }

     mnu[pmnu].nome=pstr;
     strcpy(str+pstr,p); pstr+=strlen(p)+1;

     mnu[pmnu].prev=-2;

     if (pmnu>0)
     {
      if (mnu[pmnu-1].sub==-1) mnu[pmnu-1].sub=pmnu;
      if (mnu[pmnu-1].sub!=-2) mnu[pmnu].prev=pmnu-1;
      else                     mnu[pmnu].prev=mnu[pmnu-1].prev;

      int lvl,sc;

      for (lvl=0,sc=pmnu-1; sc>-1; sc--)
      {
       if (mnu[sc].sub==-2 && mnu[sc].cmd==-2) lvl++;
       if (mnu[sc].sub!=-2) lvl--;

       if (lvl==-1) sc=-1;

       if (lvl==0 && (mnu[sc].sub!=-2 || mnu[sc].cmd!=-2))
       {
        mnu[sc].next=pmnu;
        sc=-1;
       }
      }
     }

     mnu[pmnu].sub=-1;
     mnu[pmnu].cmd=-2;
     mnu[pmnu].next=-1;

     pmnu++;
    }
    else
    if (!stricmp(p2,"endmenu"))
    {
     if (menulvl)
     {
      int sc,lvl;

      menulvl--;

      mnu[pmnu].sub=-2;  // fine menu
      mnu[pmnu].cmd=-2;  // fine menu
      mnu[pmnu].next=-1;

      for (lvl=-1,sc=pmnu-1; sc>-1; sc--)
      {
       if (mnu[sc].sub==-2 && mnu[sc].cmd==-2) lvl++;
       if (mnu[sc].sub!=-2) lvl--;

       if (lvl==0 && mnu[sc].sub!=-2) // questo e' il menu_start corrispondente
       {
        mnu[pmnu].prev=mnu[sc].prev;
        mnu[pmnu].nome=mnu[sc].nome;
        sc=-1;
       }
      }
      pmnu++;
     }
     else { errore(nome,line," : unexpected endmenu : ",ori); err=5; }
    }
    else
    if (!stricmp(p2,"item"))
    {
     {
      char *p;

      p=mstrtok(&mytok,0);
      if (!p) { errore(nome,line," : Syntax error : ",ori); err=5; }

      mnu[pmnu].nome=pstr;
      strcpy(str+pstr,p); pstr+=strlen(p)+1;

      mnu[pmnu].prev=-1;

      if (pmnu>0)
      {
       if (mnu[pmnu-1].sub==-1) mnu[pmnu-1].sub=pmnu;
       if (mnu[pmnu-1].sub!=-2) mnu[pmnu].prev=pmnu-1;
       else                     mnu[pmnu].prev=mnu[pmnu-1].prev;

       int sc,lvl;

       for (lvl=0,sc=pmnu-1; sc>-1; sc--)
       {
        if (mnu[sc].sub==-2 && mnu[sc].cmd==-2) lvl++;
        if (mnu[sc].sub!=-2) lvl--;

        if (lvl==-1) sc=-1;

        if (lvl==0 && (mnu[sc].cmd!=-2 || mnu[sc].sub!=-2))
        {
         mnu[sc].next=pmnu;
         sc=-1;
        }
       }
      }

      mnu[pmnu].sub=-2;
      mnu[pmnu].next=-1;
      mnu[pmnu].cmd=pcmd;
      pmnu++;

      // adesso devo mettere i comandi da eseguire
      while ((p=mstrtok(&mytok,0))!=0)
      {
       if (!stricmp(p,"\\"))
       { riga(buff,ori,&err,&cfg); p=mstrtok(&mytok,buff); line++; }
       if (*p!=34)
       {
        int tok;

        for (tok=0; tabcom[tok].ncom<255; tok++)
        {
         if (!stricmp(p,tabcom[tok].com))
         {
          if (err==0) { cmd[pcmd]=tabcom[tok].ncom; pcmd++; }
          break;
         }
        }
        if (err==0 && tabcom[tok].ncom==255)
        {
         errore(nome,line," : Unknown function : ",ori); err=5;
        }
       }
       else
       { // e' una stringa, la trasloco
        p++;

        if (err==0)
        {
         cmd[pcmd]=0xff; // token per "stringa"
         cmd[pcmd+1]=strlen(p); pcmd+=2;
         strcpy((char *)(cmd+pcmd),p); pcmd+=strlen(p);
        }
       } // fine gestione stringa

       if (pcmd+1024>cmdlen)
       {
        cmdlen+=10240;
        cmd=(unsigned char *)realloc(cmd,cmdlen);
       }

      } // fine ciclo input comandi
      cmd[pcmd]=0; pcmd++; // tappo

     }
    }

    else { errore(nome,line," : Unknown command : ",ori); err=5; }

    if (pstr+1024>strlun) { strlun+=10240; str=(char *)realloc(str,strlun); }

    if (pres+10>lres) { lres+=1024; res=(unsigned int **)realloc(res,lres*4); }
   } // fine riga con primo token ok
  } // fine while !feof
  mclose(&cfg);

  if (ifcount!=0) { output("\nifdef without endif detected\n"); err=5; }
  if (menulvl!=0) { output("\nmenu without endmenu detected\n"); err=5; }
//gettst3();
 }

 /* FINE */

 if (err==0) // non ci sono errori, posso abilitare il file di conf
 {
  int i;
  char *nc;

  i=0; if (configs) i=strlen(configs)+1;
  nc=new char[i+strlen(nome)+1];
  if (configs) { strcpy(nc,configs); nc[i-1]=(char)127; }
  strcpy(nc+i,nome);
  delete configs; configs=nc;

  if (config.strs==(char *)0xffffffff) { config.pstrs=0; config.strs=0; }
  if (config.cmds==(unsigned char *)0xffffffff) { config.pcmds=0; config.cmds=0; }
  if (config.keys==(unsigned int *)0xffffffff) { config.pkeys=0; config.keys=0; }
  if (config.mnus==(_menu *)0xffffffff) { config.pmnus=0; config.mnus=0; }

  key[pkey++]=0; key[pkey++]=0; // tappo !

  if (config.pcmds)
  {
   unsigned int *p=key; // riloco i puntatori ai comandi
   while(*p)
   {
    p[2]+=config.pcmds;
    p+=p[0]+3;
   }
  }

  if (pmnu) // riloco i menu
  {
   for (i=0; i<(int)pmnu; i++) // riloco i puntatori nei menu
   {
    if (config.strs) if (mnu[i].nome>0) mnu[i].nome+=config.pstrs;
    if (config.cmds) if (mnu[i].cmd>0)  mnu[i].cmd+=config.pcmds;
   }
  }

  for (i=0; i<pres; i++) *res[i]+=config.pstrs; // riloco le stringhe

  if (pkey)
  {
   if (config.keys)
   { // key merge
    config.keys=(unsigned int *)realloc(config.keys,(config.pkeys+pkey-2)*sizeof(int));
    memcpy(config.keys+config.pkeys-2,key,pkey*sizeof(int));
    config.pkeys+=pkey-2; // meno il tappo
   }
   else
   { // key create
    config.keys=new unsigned int[pkey];
    memcpy(config.keys,key,pkey*sizeof(int));
    config.pkeys=pkey;
   }
  }

  if (pcmd)
  {
   if (config.cmds)
   { // cmd merge
    config.cmds=(unsigned char *)realloc(config.cmds,config.pcmds+pcmd);
    memcpy(config.cmds+config.pcmds,cmd,pcmd);
    config.pcmds+=pcmd;
   }
   else
   { // cmd create
    config.cmds=new unsigned char[pcmd];
    memcpy(config.cmds,cmd,pcmd);
    config.pcmds=pcmd;
   }
  }

  if (pstr)
  {
   if (config.strs)
   { // str merge
    config.strs=(char *)realloc(config.strs,config.pstrs+pstr);
    memcpy(config.strs+config.pstrs,str,pstr);
    config.pstrs+=pstr;
   }
   else
   { // str create
    config.strs=new char[pstr];
    memcpy(config.strs,str,pstr);
    config.pstrs=pstr;
   }
  }

  if (pmnu)
  {
   if (config.mnus)
   { // menu merge
    for (i=0; i<(int)config.pmnus; i++) // riloco i puntatori nei menu
    {
     if (config.mnus[i].prev>0) config.mnus[i].prev+=pmnu;
     if (config.mnus[i].sub>0)  config.mnus[i].sub+=pmnu;
     if (config.mnus[i].next>0) config.mnus[i].next+=pmnu;
    }
    config.mnus=(_menu *)realloc(config.mnus,(config.pmnus+pmnu)*sizeof(_menu));
    memmove(config.mnus+pmnu,config.mnus,config.pmnus*sizeof(_menu));
    memcpy(config.mnus,mnu,pmnu*sizeof(_menu));
    config.pmnus+=pmnu;

    i=0;
    while (mnu[i].next>-1) i=mnu[i].next;
    config.mnus[i].next=pmnu;
   }
   else
   { // mnu create
    config.mnus=new _menu[pmnu];
    memcpy(config.mnus,mnu,pmnu*sizeof(_menu));
    config.pmnus=pmnu;
   }
  }
 }
 else memcpy(&config,&savecfg,sizeof(_config)); // ripristino la configurazione

 delete key; delete cmd; delete str; delete mnu;
 delete res;

 if (err>0) err+=100;

 if (err)
 {
  char *p;

  switch(err)
  {
   case 101: p="No memory to compile configuration file"; break;
   case 105: p="Syntax error(s)."; break;
   case 106: p="Config file too big"; break;

   case 102:
   case 110:
   case 103:
   case 104: p="Error: Cannot open/read !"; break;

   default : p="Unknown error"; break;
  }
  output(p); output("\n");
 }

 return(err);
}

/***************************************************************************/

void defaultok(void)
{
 char *str;
 int strlun;
 int pstr,strstart;
 int i;

 static unsigned char colordef[]={
  0x1a,0x07,0x0f,0x04,0x05,0x04,0x02,0x07,0x0f,0x02,  //  0- 9
  0x70,0x7f,0x1f,0x7f,0x0a,0x0e,0x5f,0x17,0x03,0x73,  // 10-19
  0x06,0x02,0x00,0x0e,0x03,0x03,0x05,0x03,0x03,0x03,  // 20-29
  0x04,0x00,0x05,0x06,0x03,0x06,0x0e,0x03,0x00,0x00,  // 30-39  38 e 39 liberi
  0x0e,0x02,0x0f,0x06,0x05};                          // 40-44

 static struct // default per gli interi
 {
  int *val;
  int def;
 } defint[]={{&config.maxx,0},{&config.maxy,0},{&config.vars,0},
 {&config.bak,0},{&config.whole,0},{&config.softret,32},{&config.doc,2},
 {&config.rmargin,78},{&config.lmargin,1},{&config.rient,3},{&config.indent,1},
 {&config.indcfg,0x111},
 #if defined UNIX
  {&config.ansi,1},
 #else
  {&config.ansi,0},
 #endif
 {&config.nospace,1},
 {&config.coltoscroll,5},{&config.maxdelbuf,100},{&config.showmatch,1},
 {&config.shwtab,1},{&config.tabexpl,3},{&config.tabexpr,1},
 {&config.tabsize,8},
 {&config.colormode,1},{&config.ww,15+16},
 #ifdef UNIX
  {&config.delay,100},{&config.autosave,0},
 #endif
 #if defined __linux
  {&config.shiftlinux,0},
 #endif
 {0,0}};

 static struct // default per le stringhe
 {
  int *val;
  char *def;
 } defstr[]={{&config.streof,"<<< EOF >>>"},

 {&config.streof,"<<< EOF >>>"},
 {&config.streop,"<<< EOP >>>"},
 {&config.strsop,"<<< SOP >>>"},
 {&config.streol,
  #ifdef MSDOS
   "\r\n"
  #else
   "\n"
  #endif
  },
 {&config.strmail,
  #ifdef MSDOS
   "golded.msg"
  #else
   "Mail/tmp/snd."
  #endif
  },
 {&config.strfnddef,"I"},
 {&config.strifnddef,"I"},
 {&config.strrpldef,"I"},

 #ifdef X11
  {&config.display,""},
  {&config.geometry,""},
  {&config.font,"9x15"},
 #endif
 {0,0}};

 pstr=0;
 strlun=2048;
 str=new char[strlun];
 strstart=config.pstrs;

 // default per i colori

 for (i=0; i<MAXCOLORS; i++)
 {
  if (config.col[i]==-1) config.col[i]=colordef[i];
  if (config.col[i]== 0 && i>20) config.col[i]=-1;
 }

 // default per gli interi

 for (i=0; defint[i].val; i++)
  if (*defint[i].val==-1) *defint[i].val=defint[i].def;

 if (config.maxx) config.fixx=1; else config.fixx=0;
 if (config.maxy) config.fixy=1; else config.fixy=0;

 // default per le stringhe

 for (i=0; defstr[i].val; i++)
 {
  if (*defstr[i].val==-1)
  {
   strcpy(str+pstr,defstr[i].def);
   *defstr[i].val=pstr+strstart;
   pstr+=strlen(defstr[i].def)+1;
  }
 }

 #if defined __linux && defined X11
  config.shiftlinux=0;
 #endif

 /* COSTRIZIONI */

 #ifdef UNIX
  if (config.ansi<1 || config.ansi>3)
   config.ansi=1; // forzo modo ansi color se UNIX (ok 1,2,3.)
 #endif

 if (config.doc==1 || config.showmatch || config.colormode)
 { config.shwtab=1; config.nospace=1; }

 if (config.tabexpr && config.tabexpl) config.shwtab=1;

 if (config.strs)
 { // str merge
  config.strs=(char *)realloc(config.strs,config.pstrs+pstr);
  memcpy(config.strs+config.pstrs,str,pstr);
  config.pstrs+=pstr;
 }
 else
 { // str create
  config.strs=new char[pstr];
  memcpy(config.strs,str,pstr);
  config.pstrs=pstr;
 }
 delete str;
}

/***************************************************************************/

char *retversion(void)
{
 return(maindata.version);
}

/***************************************************************************/

char *retstvers(void)
{
 return(maindata.stvers);
}

/***************************************************************************/

char *mstrtok(toks *conf,char *s1,unsigned int *xkeys)
{
// conf->static char *conf->st;
// conf->static int conf->pos;
 int ret;
 char end;

 if (s1>(char *)1) { conf->st=s1; conf->pos=0; } // inizializzazione:
 if (!conf->st) return(0);

 if (s1==(char *)1)
 {
  char *ret=conf->st+conf->pos;
  conf->st=0; conf->pos=0;
  return(ret);
 }
 else if (xkeys==0)
 {
  for (; conf->st[conf->pos]==' ' && conf->st[conf->pos]!='\0'; conf->pos++); // cerco il primo non spazio
  ret=conf->pos;

  if (conf->st[conf->pos]=='"') { conf->pos++; end='"'; } else end=' '; // se e' una conf->stringa

  for (; conf->st[conf->pos]!=end && conf->st[conf->pos]!='\0'; conf->pos++)
  {
   if (conf->st[conf->pos]=='\\')
   {
    char nx=conf->st[conf->pos+1];

    if (nx=='"' || (nx=='\\' && end=='"')) strcpy(conf->st+conf->pos,conf->st+conf->pos+1);
    else
    if (isxdigit(conf->st[conf->pos+1]) && isxdigit(conf->st[conf->pos+2]))
    {
     conf->st[conf->pos]=(htoi(conf->st[conf->pos+1])<<4)|htoi(conf->st[conf->pos+2]);
     if (conf->st[conf->pos]=='&' && end==' ')
     {
      conf->st[conf->pos+1]=conf->st[conf->pos];
      conf->st[conf->pos]='\\';
      strcpy(conf->st+conf->pos+2,conf->st+conf->pos+3);
     }
     else
     if (conf->st[conf->pos]=='\0')
     {
      conf->st[conf->pos+1]='0';
      conf->st[conf->pos]='\\';
      strcpy(conf->st+conf->pos+2,conf->st+conf->pos+3);
     } else strcpy(conf->st+conf->pos+1,conf->st+conf->pos+3);
    }
    else
    if (isxdigit(conf->st[conf->pos+1]))
    {
     if (htoi(conf->st[conf->pos+1])!=0)
     { conf->st[conf->pos]=htoi(conf->st[conf->pos+1]); strcpy(conf->st+conf->pos+1,conf->st+conf->pos+2); }
    }
   }
  }

  if (!conf->st[ret]) { conf->st=0; ret=0; } else
  { if (conf->st[conf->pos]) conf->st[conf->pos++]='\0'; }
  return(conf->st+ret);
 }
 else
 {
  unsigned int len=0,shst=0;
  unsigned int *keys=&xkeys[3];

  while(conf->st[conf->pos]!=' ' && conf->st[conf->pos]!='\0' && len+8<MAXXKEYS)
  {
   if (conf->st[conf->pos]=='\\')
   {
    if (conf->st[conf->pos+1]=='l' ||
       (isxdigit(conf->st[conf->pos+1]) && isxdigit(conf->st[conf->pos+2]) &&
        isxdigit(conf->st[conf->pos+3]) && isxdigit(conf->st[conf->pos+4]) &&
        isxdigit(conf->st[conf->pos+5]) && isxdigit(conf->st[conf->pos+6]) &&
        isxdigit(conf->st[conf->pos+7]) && isxdigit(conf->st[conf->pos+8])))
    {
     if (conf->st[conf->pos+1]=='l') conf->pos++;
     *keys=(htoi(conf->st[conf->pos+1])<<28)|(htoi(conf->st[conf->pos+2])<<24)|
           (htoi(conf->st[conf->pos+3])<<20)|(htoi(conf->st[conf->pos+4])<<16)|
           (htoi(conf->st[conf->pos+5])<<12)|(htoi(conf->st[conf->pos+6])<<8)|
           (htoi(conf->st[conf->pos+7])<< 4)| htoi(conf->st[conf->pos+8]    );
     len++; keys++;
     conf->pos+=9;
    }
    else
    if (conf->st[conf->pos+1]=='w' ||
       (isxdigit(conf->st[conf->pos+1]) && isxdigit(conf->st[conf->pos+2]) &&
        isxdigit(conf->st[conf->pos+3]) && isxdigit(conf->st[conf->pos+4])))
    {
     if (conf->st[conf->pos+1]=='w') conf->pos++;
     *keys=(htoi(conf->st[conf->pos+1])<<12)|(htoi(conf->st[conf->pos+2])<<8)|
           (htoi(conf->st[conf->pos+3])<< 4)| htoi(conf->st[conf->pos+4]    );
     len++; keys++;
     conf->pos+=5;
    }
    else
    if (conf->st[conf->pos+1]=='u' || (isxdigit(conf->st[conf->pos+1]) && isxdigit(conf->st[conf->pos+2])))
    {
     if (conf->st[conf->pos+1]=='u') conf->pos++;
     *keys=(htoi(conf->st[conf->pos+1])<<4)|htoi(conf->st[conf->pos+2]);
     len++; keys++;
     conf->pos+=3;
    }
    else
    {
     *keys=(unsigned int)(unsigned char)conf->st[conf->pos+1];
     len++; keys++;
     conf->pos+=2;
    }
   }
   else if (conf->st[conf->pos]=='&') // e' uno shift status
   {
    conf->pos++;
    do
    {
     switch(conf->st[conf->pos])
     {
      case 't' : shst|=64; conf->pos++; break;    // timed operation

      case 'a' : shst|=256; conf->pos++; break;   // flags operation
      case 'A' : shst|=512; conf->pos++; break;
      case 'b' : shst|=1024; conf->pos++; break;
      case 'C' : shst|=2048; conf->pos++; break;
      case 'd' : shst|=4096; conf->pos++; break;
      case 'D' : shst|=8192; conf->pos++; break;
      case 'e' : shst|=16384; conf->pos++; break;
      case 'E' : shst|=32768; conf->pos++; break;
      default  : len=0xffffffff;                    // unknown shift status
//        errore(line," : unknown shift status \n\r   ",ori); err=5;
       conf->pos++; break;
     }
    } while(conf->st[conf->pos]!=' ' && conf->st[conf->pos]!='\0'); // ciclo fino alla fine
    break; // esco dal while (fine della stringa key)
   }
   else
   {
    *keys=(unsigned int)(unsigned char)conf->st[conf->pos];
    len++; keys++;
    conf->pos+=1;
   }
  }

  xkeys[0]=len; xkeys[1]=shst; xkeys[2]=0;
  return(0);
 }
}

/***************************************************************************/

