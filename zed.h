// per configurare, definire o MSDOS o UNIX
// non serve definire MSDOS perche' il DJGPP gia' lo definisce

#ifndef MSDOS
 #define UNIX
#else
 #undef UNIX
#endif

/***************************************************************************/

#ifdef UNIX
 #define stricmp strcasecmp
 #define strnicmp strncasecmp
#endif

/***************************************************************************/
/* CONFIG 1 */
/***************************************************************************/

#define MAXXKEYS 500
#define MAXCOLORS 45

struct _menu
{
 int nome; // puntatore al nome in strs
 int prev; // puntatore al menu' precedente in mnus
 int sub;  // puntatore al sottomenu' in mnus
 int cmd;  // puntatore ai comandi in cmds
 int next; // puntatore alla prossima voce in mnus
};

struct _config
{
 int nospace;     // se true elimino gli spazi e i tab alla fine riga
 int coltoscroll; // colonne da scrollare lateralmente
 int maxx;        // numero colonne del video, se 0=autodetect
 int maxy;        // numero righe del video, se 0=autodetect
 int fixx,fixy;   // x e y autodetect o no ?
 int maxdelbuf;   // numero massimo di linee da mantenere nel delbuffer
 int showmatch;   // mostra le parentesi associate
// int curmod;      // modifica cursore se cambio inserimento
 int whole;       // riscrive sempre tutta la riga
 unsigned int *keys;    // pointer alle sequenze di tasti
 unsigned char *cmds;   // pointer alle sequenze di comandi
 char *strs;            // pointer alle stringhe
 _menu *mnus;           // pointer al menu
 int pkeys;    // lunghezza buffer tasti
 int pcmds;    // lunghezza buffer comandi
 int pstrs;    // lunghezza buffer stringhe
 int pmnus;    // lunghezza buffer menu
 int col[MAXCOLORS];// colorini vari
 int streof;      // stringa di EOF
 int streop;      // stringa di EOP
 int strsop;      // stringa di SOP
 int streol;      // stringa di EOL
 int strfnddef;   // default per : find
 int strrpldef;   //               replace
 int strifnddef;  //               ifind
 int strmail;     // stringa per identificare la mail
 int ansi;        // ansi/fossil mode ?
 int indent;      // autoindent
 int indcfg;      // configurazione indent
 int shwtab;      // mostra o espandi i tab
 int tabexpl;     // espandi i tab in loading
 int tabexpr;     // espandi i tab run-time
 int rmargin;     // right margin
 int lmargin;     // left margin
 int rient;       // rientranza
 int doc;         // 1 se modo documento
 int softret;     // codice del soft return
// int strpen[8];   // stringe di definizione colori per HPterm
 int bak;         // genera .bak (~ sotto unix) o no
 int vars;        // user flags
 int autosave;    // autosave
 int colormode;   // colormode: 0=non colora, 1=colora i c cc C h cpp
                  // 2=colora tutto
#ifdef UNIX
 int delay;       // ritardo per la temporizzazione in millisecondi
 int shiftlinux;  // uso TIOCLINUX/6 ?
#endif
#ifdef X11
 int font;        // font da usare per X11
 int display;     // display da aprire per X11
 int geometry;    // specifica per X11
 int xcol[16];    // stringhe dei colori per X11 richiesti dall'utente
#endif
 int gotoline;    // eventuale goto line iniziale
 int config;      // eventuale richiesta di altro config file
 int ww;          // configurazione del wordwrap
 int tabsize;     // dimensione di un tab (default 8)
};

extern _config config;
extern char *configs;

struct commands // struttura dei comandi
{
 char *com;
 unsigned char ncom;
};

extern commands tabcom[];

//extern int fileok;
extern int myok;

/***************************************************************************/
/* LISTA - definizione della classe "list" */
/***************************************************************************/

class editor;

class list
{
 protected:

 class elem // classe del singolo elemento
 {
  private:
  elem *prec;  // puntatore al precedente
  elem *pros;  // puntatore al prossimo
  int lung;
  char dati[0];  // puntatore ai dati

  friend editor; // solo pero' all'interno di ZED.H
  friend list;
 };

 protected:

 elem *prima;  // punta al primo della lista
 elem *corr;   // punta al corrente nella lista
 elem *ulti;   // punta all'ultimo della lista
 int lcorr;   // numero linea corrente (da 0)
 int lulti;   // numero dell'ultima linea (tot righe -1) (da 0)

 public:
 int add(char *data,int len,int modo);
 void remove(void );
 int subst(char *data,int len);
 int isubst(elem **sc,char *data,int len);
 void startup(void);
 void eraseall(void);
 list(void) { startup(); }
 ~list(void) { eraseall(); }
 void delfirst(void) { corr=prima; lcorr=0; remove(); }
 void gotofirst(void) { corr=prima; lcorr=0; }
 void gotolast(void) { corr=ulti; lcorr=lulti; }

 void next(void)
 {
  if (corr->pros) { corr=corr->pros; lcorr++; }
  else { corr=prima; lcorr=0; }
 }

 elem *irprev(elem *mio) { return(mio->prec); }
 elem *irnext(elem *mio) { return(mio->pros); }
 elem *crprev(void) { return(corr->prec); }
 elem *crnext(void) { return(corr->pros); }

 void prev(void)
 {
  if (corr->prec) { corr=corr->prec; lcorr--; }
  else { corr=ulti; lcorr=lulti; }
 }
 int sseek(char *str);

 char* irdati(elem *mio) { return(mio->dati); }
 char* rdati(void) { return(corr->dati); }
 int rlung(void) { return(corr->lung); }
 int irlung(elem *mio) { return(mio->lung); }
 int rlcoor(void) { return(lcorr); }

 int rtot (void) { return((lulti==0xfffffff)?0:(int)lulti+1); }
 int nseek(int edn);
 int inseek(elem **scorr,int &slcorr,int edn);
 int aseek(char *fd,int fl);
};

/***************************************************************************/
/* EDITOR - definizione della classe "editor" */
/***************************************************************************/

#define MAXLINE 1024 // massima lunghezza della riga, NON MODIFICARE !

#define FL_NEWFILE 1  // flag di "new file"
#define FL_MODIF 2    // flag di "file modificato"
#define FL_RMODIF 4   // flag di riga modificata
#define FL_SOVR 8     // flag di sovrascrittura
#define FL_BUFFER 16  // flag di "riga in buffer in editazione"
#define FL_BCOL 32    // flag di "blocco a colonna"
#define FL_BLDR 64    // flag di "blocco drag a linee"
#define FL_BCDR 128   // flag di "blocco drag a caratteri"
#define FL_APPEND 256 // flag di "al prossimo save fai un append"
#define FL_SCRITT 512 // flag di "lascia le scritte nell'editor"

/***************************************************************************/

struct stat;

class editor : public list
{
 friend void signals(int i);
 friend void gopipe(editor *p);

 public:

 char filename[128];  // nome del file in editazione
 int timeouttosave;

 private:
 short int flag;      // flag vari
 short int yda;       // inizio video y
 short int ya;        // fine video y
 short int x;         // posizione reale nel buffer
 short int vx;        // posizione video x
 short int vidy,vidx; // posiz video x,y
 char buffer[MAXLINE];// pointer al line buffer corrente
 short int lbuf;      // lunghezza del line buffer
 short int soft;      // la riga nel buffer ha return soft ?
 short int todraw;    // prime cose dell'header da riscrivere
 short int redraw;    // redraw elem ?
 int posiz[4];       // segnalini 0..3
 short int posizx[4]; // segnalini 0..3
 int oldcor;         // oldcurrent per ricalcolo di pari e pare
 int pari;           // riga di inizio paragrafo
 int pare;           // riga di fine paragrafo
 int part;           // in memoria c'e' solo una parte di file
 char *ppbuf;        // pipe buffer
 int pppos;          // posizione nel pipe buffer
 int colorize;       // coloro ? 0=no, 1=si
 int binary;         // binary mode?
 int readonly;       // readonly mode?
 char *mailquote;    // mailquote
 int mailpos;        // ex posizione nella mail
 int docmode;        // attivazione modo documento

 int coloracpp(unsigned short int *buf,char *str,int vidx,int len,int ss);
 int colorahtml(unsigned short int *buf,char *str,int vidx,int len,int ss);
 int coloratex(unsigned short int *buf,char *str,int vidx,int len,int ss);
 int coloramail(unsigned short int *buf,char *str,int vidx,int len/*,int ss*/);
 int colorajava(unsigned short int *buf,char *str,int vidx,int len,int ss);
 void started(void);
 void update(void);
 void putres(int y,elem *scorr,int n);
 void putelm(elem *mcr,int n,int y);
 void putpag(void);
 void putdaa(int rdda,int rda);
 void blockok(void);
 int isword(int ch);
 void blkcend(int x);
 int doblockclip(int m);
 int doclipins(int m);
 void okdelbuf(void);
 void find(int modo);
 void exportclip(int m,int my);

 int reginstr(char *str,int len,int *slen,int start,int end,int back,regex_t *regex);
 int meminstr(char *str,char *sstr,int len,int slen,int start,int end,int typ);
 int vtor(int r,elem *elm);
 int rtov(int r);
 int tabexp(char *b,short int *l);

 int irlung(elem *mio) { return(mio->lung&0x03ff); }

 int softret(void) { return(corr->lung&0x4000); }
 int isoftret(elem *mio) { return(mio->lung&0x4000); }
 void isetsoft(elem *mio,int set)
 {
  if (set) mio->lung|=0x4000;
  else     mio->lung&=0xbfff;
 }
 void setsoft(int set)
 {
  if (set) corr->lung|=0x4000;
  else     corr->lung&=0xbfff;
 }
 void gotol(int line);

 int isetcol(elem *mio,int stato);
 int setcol(int stato);
 int colret(void) { return((corr->lung&0x1c00)>>10); }
 int icolret(elem *mio) { return((mio->lung&0x1c00)>>10); }

 public:

 void ydaset(int yy) { yda=yy; } // usato per i msg per la clipboard !
 int rlung(void) { return(corr->lung&0x03ff); }
 void setcur(void);
 int pipefd;          // file descriptor della pipe
 struct stat stato;   // la struttura stat
 void reset(void);
 void setpipe(int fd);
 void word(char *str);
 int askvidx(void) { return(vx-vidx); }
 int askvidy(void) { return(vidy); }
 void orflag(int f) { flag|=f; }
 void andflag(int f) { flag&=f; }
 void settodraw(int f) { todraw=f; }
 void setyda(int y) { yda=y; }
 char *rfilename(void ) { return(filename); }
 int ydaret(void) { return(yda); }
 int execcmd(int cmd);
 void execch(int ch);
 void importclip(int w,int p);

 int add(char *data,int len,int modo);
 int subst(char *data,int len);

 int createhelp(void);
 int loadfile(char *nome,int all);
 int savefile(void );
 void getup(int yda1,int ya1);
 void back(void);
 editor(int yda1,int ya1);
 ~editor(void );
 void puthdr(int cosa);
 int looptst(void);
 int modif(void) { return(flag&FL_MODIF); }
 void setmodif(void);
 int colcheck(elem *scorr,int type=0,char *str1=0,char *str2=0);
 int getreadonly(void) { return(readonly); }
};

/***************************************************************************/
/* CONFIG 2 */
/***************************************************************************/

#define MAXVIDX 514 // massima larghezza video

struct _block // struttura per i blocchi
{
 int blk;         // boolean : blocco selezionato/non selezionato
 int type;        // 0:a linee / 1:a colonne
 editor *edtr;    // editor possessore del blocco

 int start;      // numero linea di inizio
 int cstart;      // carattere di start all'interno della riga

 int end;        // numero linea di fine
 int cend;        // carattere di fine all'interno della riga
};

struct _clip
{
 editor *edtr;
 int blktype;
 int cstart,cend;
 int pastefrom;
};

struct _match
{
 int ox,oy;
 int x;
 int y;
 short int save;
};

struct _modif
{
 char modif;
 int da;
 int a;
};

/* colori vari:
   0 : colore riga di stato
   1 : colore riga non corrente
   2 : colore riga corrente
   3 : colore riga di End Of File
   4 : colore delle cornici dei dialog
   5 : colore degli errori gravi (es. out of memory)
   6 : colore avvertimenti normali (es. "new file")
   7 : colore selezione non corrente
   8 : colore selezione corrente
   9 : colore riquadro selezioni
   10: colore blocco selezionato
//   11: colore copy cursor
   12: colore evidenz. testo trovato
   13: colore riga corrente dentro blocco
   14: colore del match
   16: colore control code ansi
   17: colore parametri di default/ultimo testo immesso
   18: colore paragrafo corrente
   19: colore paragrafo corrente in blocco

   colorazione sorgenti c++:

   20: colore commenti
   21: colore preprocessore
   22: colore simboli
   23: colore identificatori
   24: colore costanti decimali
   25: colore costanti esadecimali
   26: colore costanti ottali
   27: colore costanti float
   28: colore stringhe
   29: colore costanti char
   30: colore errori
   31: colore parole riservate

   colorazione html:

   32: colore comandi
   33: colore parametri dei comandi
   34: colore delle costanti assegnate

   colorazione tex:

   35: colore commenti
   36: colore testo math mode
   37: colore comandi

   38: libero
   39: libero

   colorazione mail:

   40: colore linea quotata
   41: colore control line
   42: colore tag line
   43: colore tear line
   44: colore origin
*/

extern int pwdln;
extern char *s_oom;
extern short int *vbuf; // usato da putelm di editor
extern _modif *astat;
extern short int *avideo;
extern _block block;
extern _clip clip;
extern _match match;
extern editor *delbuf;
//extern char *version;
//extern char *stvers;
//extern int versionl;
extern char *cmpbuf;

int lconfig(int quale,char *nome);
int cconfig(char *nome);
int htoi(char ch);
void defaultok(void);
int parsecm(int argc,char *argv[],char *nome);
void outtestata(void);
char *retversion(void);
char *retstvers(void);

/***************************************************************************/
/* DIALOG - gestione della classe "dialog" */
/***************************************************************************/

#define FL_NEWFILE 1 // flag di "new file"
#define FL_MODIF 2   // flag di "file modificato"
#define FL_RMODIF 4  // flag di riga modificata
#define FL_SOVR 8    // flag di sovrascrittura
#define FL_BUFFER 16 // flag di "riga in buffer in editazione"

/***************************************************************************/

class dialog
{
 private:

 char *buffer;       // pointer al line buffer corrente
 int lbuf;           // lunghezza del line buffer
 int flag;
 int ly;
 int x;
 int first;
 int tabexit;
 int filespc;

 void putstr2(char *str2,int evi=0);
 int execcmd(int cmd);
 void execch(int ch);
 int looptst(void );

 public:

 dialog(void ) { flag=0; }
 int ask(char *str,int yda,char *instr,int special=0);
 int retpos(void) { return(ly); }
};

/***************************************************************************/
/* SELEZ - definizione della classe "selez" */
/***************************************************************************/

#define MAXLINE 1024 // massima lunghezza della riga

/***************************************************************************/

class selez : public list
{
 public:
 int xda,xa,yda,ya;  // bordi finestra
 int cy;             // current y
 private:
 char title[80];     // titolo della selezione
 int tipo;           // =1=file=abilitazione open_all =2=menu

 void putres(int y,elem *scorr);
 void putstr2(char *str2,int l,int y,int col);
 void putpag(void);

 public:
 void setcur(int x,int y);
 selez(char *str);

 int add(char *data,int len,int modo) { return(list::add(data,len,modo)); }

 int sseek(char *str) { return(list::sseek(str)); }
 int nseek(int n) { return(list::nseek(n)); }
 int goselez(char *str,int y,int file);

 int execcmd(int cmd);
 int execch(int ch);
 int looptst(void);
};

/***************************************************************************/

editor *askfile(int argc,char *argv[],int yda,int ya,editor *sedt);
int gomenu(int start,int xx);

/***************************************************************************/
/* LOWL */
/***************************************************************************/

#define BUFFSIZE (65520U)

struct mfile
{
 char *buf;   // ibuff
 int h;       // ifile
 int eof;     // fleof
 int len,pos; // blen bpos
};

struct svsave
{
 int ya,yda,xmax;
 char *buf;
};

#ifdef MSDOS
 int _chdir(char *path);
#else
 #define _chdir chdir
 void strlwr(char *str);
#endif
void _getcwd(char *path,int max,char drive=0);
int querykey(void);
void setcurtype(int t);
void output(char *str);
inline void outansi(char *str) { output(str); }
void outint(int i);
void clearscr(void );
void saveline(int yy,char *save);
void restline(int yy,char *save);
int  init(int upd);
void printlogo(void);
void preinit(void);
void deinit(void);
void putint(int ii,int n,int x,int y,short int col);
void putchr(unsigned char chr,int x,int y,short int col);
void scroll(int yda,int ya,int yd);
void putstr(char *str2,int fi,int x,int y,short int col);
void vputstr(char *str2,int fi,int y,short int col);
void vsetcur(int x,int y,int update=0);
int  chksh(int ch);
void refresh(void);
unsigned int gettst(void);
unsigned int gettst2(void);
void gettst3(void);
char confirm(char *sstr,char *keys,int yy,short int col);
int  exectst(int *dato);
void vsavecur(int *x,int *y);
void vrestcur(int &x,int &y);
void putchn(char ch,int fi,int x,int y,short int col);
svsave *vsave(int yda,int ya);
void vrest(svsave *ss);
int mopen(char *nome,mfile *mf);
int mread(char **str,int maxlen,int *error,int *len,mfile *mf);
void mclose(mfile *mf);
char itoh(int i);
void astatok(int yda,int ya,int xda,int xa);
void okstatus(int y);
int my_open(char *nome,int flag);
int my_access(char *nome);
void startmacro(int yy);
void abortmacro(void);
void recmacro(void);
void writemacro(int yy);
int memicmp(char *p1,char *p2,int ln);

/***************************************************************************/
/* MAIN */
/***************************************************************************/

void signals(int i);
void mainswitch(int w);
void wup(int type=0);
int main(int argc,char *argv[]);
editor *askfile(int argc,char *argv[],int yda,int ya,editor *sedt);
void okpath(void);
editor *bufsel(int yda);
void gopipe(editor *p);
void goresize(void);
void chkwinsize(void);

void winchk(editor *edtr);
struct _wind { int myda,mya; editor *edtr; };
struct _mouse
{
 int togox,togoy;

 int x,y;
 int px,py;
 int rx,ry;
 int mot;
 int win;

 int exec;
};

// low 8 bit: y di fine del dialog
//  256 9o bit: 1 se dentro un dialog
//  512 10o bit: 1 se devo abortire il dialog
// 1024 11o bit: 1 se il dialog in uscita deve fare un wup.

extern char *s_str;
extern char *s_strto;
extern char *s_stropti;
extern char *s_stropts;
extern char *s_stropta;
extern short int s_flag;

#ifdef MSDOS
 extern char *argv0;
#endif
extern _wind *win;
extern int cwin,winn,savecwin;
extern list edt;
extern editor *nedt;
extern char *macrocmd;
extern int macropos;
extern int macrostop;
extern int pipes;
extern int dialof;
extern int stopupdate;
extern _mouse mouse;
extern selez *crtselez;
extern dialog *crtdialog;

#ifdef MSDOS
 extern int vidsel;
 extern int vidoff;
 extern int vidmyds;
#endif

/***************************************************************************/

int do_exec (char *xfn, char *pars);

/***************************************************************************/
/* X11PART */
/***************************************************************************/

#ifdef X11

typedef struct _win_info
{
 int pwidth,pheight;  // window width and height in pixels
 int cwidth, cheight; // window width and height in characters
 int fwidth, fheight; // font width and height in pixels
 int fascent;
 int saved_lines;     // total lines to save in scroll-back buffer
 int offset;          // how far back we are in the scroll back buffer
 int sline_top; // high water mark of saved scrolled lines
} WindowInfo;

#define MARGIN 2

int init_display(void);
unsigned int getxkey(void);
int getxsh(void);
void okmouse(void);

struct _select
{
 char *str;
 int pos;
 int len;
 Time time;
};

extern Display *display;
extern WindowInfo MyWinInfo;
extern GC gcontext[MAXCOLORS];
extern Window main_win;
extern Window zed_win;
extern XFontStruct *mainfont;           // main font structure
extern int zedmapped; // a 1 se la finestra e' stata creata
extern int zedexpose; // a 1 se completamente visibile
extern int zedfocus;
extern GC gcursor;
extern int fd_x;
extern _select sele;

#endif

/***************************************************************************/

