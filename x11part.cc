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

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <regex.h>
#ifdef _AIX
 #include <sys/select.h>
 #include <strings.h>
#endif

#ifdef XPM_ICON
 #include <xpm.h>
#endif

#ifdef XPM_ICON
 #include "zedico.xpm"
#endif

#include "zed.h"

/***************************************************************************/
// variabili globali

Display *display;
WindowInfo MyWinInfo;
GC gcontext[MAXCOLORS];
Window main_win;
Window zed_win;
XFontStruct *mainfont;           // main font structure
int zedmapped=0;
int zedexpose=0;
int zedfocus=1;
int xcolormode=1;
GC gcursor;
int fd_x;
_select sele={0,0,0};

/***************************************************************************/

void createwin(void);

/***************************************************************************/

int      screen;
Colormap colormap;
unsigned int xcolor[16];
int xcolora[16]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
static Atom wm_del_win;

XSizeHints sizehints=
{
 PMinSize | PMaxSize | PResizeInc | PBaseSize | PWinGravity,
 0, 0, 80, 50,                   // x, y, width and height
 80,4,                           // Min width and height
 MAXVIDX-2, MAXVIDX-2,           // Max width and height
 1, 1,                           // Width and height increments
 {0, 0}, {0, 0},                 // Aspect ratio - not used
 2 * MARGIN, 2 * MARGIN,         // base size
 NorthWestGravity                // gravity
};

#define ZED_EVENTS (ExposureMask|ButtonPressMask|ButtonReleaseMask|\
                    ButtonMotionMask)
#define MW_EVENTS  (KeyPressMask|FocusChangeMask|StructureNotifyMask|\
                    VisibilityChangeMask)

/***************************************************************************/

void freecolgc(void)
{
 XGCValues gc;
 int i;

 for (i=0; i<MAXCOLORS; i++)
 {
  if (gcontext[i] &&
      XGetGCValues(display,gcontext[i],GCBackground|GCForeground,&gc))
  {
   XFreeGC(display,gcontext[i]);
   gcontext[i]=0;
  }
 }

 if (gcursor &&
     XGetGCValues(display,gcursor,GCBackground|GCForeground,&gc))
 {
  XFreeGC(display,gcontext[i]);
  XFreeColors(display,colormap,&gc.foreground,1,0);
  XFreeColors(display,colormap,&gc.background,1,0);
  gcursor=0;
 }

 for (i=0; i<16; i++) if (xcolora[i])
 { XFreeColors(display,colormap,(long unsigned int *)&xcolor[i],1,0); xcolora[i]=0; }
}

/***************************************************************************/

int alloccol(void)
{
 char *stdcol[16]={"#000000000000","#0000aa","#00aa00","#00aaaa","#aa0000",
                   "#aa00aa","#aa5500","#aaaaaa","#555555","#5555ff","#55ff55",
                   "#55ffff","#ff5555","#ff55ff","#ffff55","#ffffffffffff"};

 int err=0,i;
 XColor xcl,exa;

 freecolgc();

 Visual *visual=DefaultVisual(display,screen);

 if (visual->c_class==StaticGray || visual->c_class==GrayScale) err=1;
 else
 {
  for (i=0; i<16 && err==0; i++)
  {
   char *cl=0;

   if (config.xcol[i]==-1) cl=stdcol[i];
    else cl=config.strs+config.xcol[i];
   if (XAllocNamedColor(display,colormap,cl,&xcl,&exa)==0)
   {
    if (cl!=stdcol[i])
    {
     output("Can't allocate color \"");
     output(cl);
     output("\".\n");
    }
    else { if (err==0) output("Can't allocate color(s).\n"); }
    err=1;
   }
   else { xcolor[i]=xcl.pixel; xcolora[i]=1; }
  }
 }

 if (err) // caso fisso B/N
 {
  XColor xcl,exa;

  err=0;
//  output("Trying monocrome...\n");

  xcolormode=0;
  if (XAllocNamedColor(display,colormap,"#000000000000",&xcl,&exa)==0) err=1;
  else { xcolor[0]=xcl.pixel; xcolora[0]=1; }
  if (XAllocNamedColor(display,colormap,"#ffffffffffff",&xcl,&exa)==0) err=1;
  else { xcolor[15]=xcl.pixel; xcolora[15]=1; }

  char bnc[MAXCOLORS+1]=" xxxx xx x     x  xx";

  for (i=0; i<MAXCOLORS; i++)
   if (bnc[i]==' ') config.col[i]=0xf0; else config.col[i]=0x0f;

  if (err) { output("Can't allocate color(s).\n"); return(2); }
 }
 return(0);
}

/***************************************************************************/

void allocgc(void)
{
 int i;
 XGCValues gc;

 for (i=0; i<MAXCOLORS; i++)
 {
  if (config.col[i]>-1)
  {
   gc.foreground=xcolor[(config.col[i]&0x0f)];
   gc.background=xcolor[(config.col[i]&0xf0)>>4];
   gc.font=mainfont->fid;
   gcontext[i]=XCreateGC(display,main_win,GCForeground|GCBackground|GCFont,&gc);
   config.col[i]=i;
  } else gcontext[i]=0;
 }

 gc.foreground=xcolor[0]; gc.background=xcolor[15];
 gc.font=mainfont->fid;

 gcursor=XCreateGC(display,main_win,GCForeground|GCBackground|GCFont,&gc);
}

/***************************************************************************/
// ret=0=ok 1=error;

int init_display(void)
{
 int i;
 char *display_name;

 for (i=0; i<MAXCOLORS; i++) gcontext[i]=0;
 gcursor=0;

 display_name=XDisplayName(config.strs+config.display);

 if (!(display=XOpenDisplay(display_name)))
 {
  output("Can't open display:");
  output(display_name);
  output("\n\r");
  return(1);
 }

// XSynchronize(display,True); // per debugging

 fd_x=XConnectionNumber(display);

 screen=DefaultScreen(display);

 colormap=DefaultColormap(display,screen);

 if ((mainfont=XLoadQueryFont(display,config.strs+config.font))==NULL)
 {
  output("can't access font"); output(config.strs+config.font); output("\n\r");
  return(1);
 }

 int x=0,y=0,flags=0;
 unsigned int width=0,height=0;

 if (XTextWidth(mainfont,"i",1)!=XTextWidth(mainfont,"X",1))
 { output("Font proportional !\n\r"); return(1); }

 sizehints.width_inc=XTextWidth(mainfont,"M",1);
 sizehints.height_inc=mainfont->ascent+mainfont->descent;
 flags=XParseGeometry(config.strs+config.geometry,&x,&y,&width,&height);

 if (!(flags&WidthValue) && config.maxx)
 { flags|=WidthValue; width=config.maxx; }
 if (!(flags&HeightValue) && config.maxy)
 { flags|=HeightValue; height=config.maxy; }

 config.maxx=0; config.maxy=0; config.fixx=0; config.fixy=0;

 if (flags&WidthValue)  { sizehints.width=width; sizehints.flags|=USSize; }
 if (flags&HeightValue) { sizehints.height=height; sizehints.flags|=USSize; }

 MyWinInfo.fheight=sizehints.height_inc;
 MyWinInfo.fwidth =sizehints.width_inc;
 MyWinInfo.fascent=mainfont->ascent;
 MyWinInfo.cwidth =sizehints.width;
 MyWinInfo.cheight=sizehints.height;
 MyWinInfo.pwidth =MyWinInfo.cwidth*MyWinInfo.fwidth;
 MyWinInfo.pheight=MyWinInfo.cheight*MyWinInfo.fheight;

 sizehints.base_width=MARGIN*2;
 sizehints.base_height=MARGIN*2;
 sizehints.width=sizehints.width*sizehints.width_inc;
 sizehints.height=sizehints.height*sizehints.height_inc;
 sizehints.min_width =sizehints.min_width *sizehints.width_inc +sizehints.base_width;
 sizehints.min_height=sizehints.min_height*sizehints.height_inc+sizehints.base_height;
 sizehints.max_width =sizehints.max_width *sizehints.width_inc +sizehints.base_width;
 sizehints.max_height=sizehints.max_height*sizehints.height_inc+sizehints.base_height;

 if (flags&XValue)
 {
  if (flags&XNegative)
  {
   x=DisplayWidth(display,screen)+x-sizehints.width-2;
   sizehints.win_gravity=NorthEastGravity;
  }
  sizehints.x=x;
  sizehints.flags|=USPosition;
 }

 if (flags&YValue)
 {
  if (flags&YNegative)
  {
   y=DisplayHeight(display,screen)+y-sizehints.height-2;
   sizehints.win_gravity=SouthWestGravity;
   if((flags&XValue)&&(flags&XNegative))
    sizehints.win_gravity=SouthEastGravity;
  }
  sizehints.y=y;
  sizehints.flags|=USPosition;
 }

 if (alloccol()) return(1);

 createwin();
 allocgc();

  //  Enable the delete window protocol.
 wm_del_win=XInternAtom(display,"WM_DELETE_WINDOW",False);
 XSetWMProtocols(display,main_win,&wm_del_win,1);

 XSync(display,0);

 return(0);
}

//  Open and map the window.

/***************************************************************************/

void createwin(void)
{
 XWMHints wmhints;
 Cursor cursor;
 XClassHint myclass;
 Pixmap icon_pixmap;

 main_win=XCreateSimpleWindow(display,DefaultRootWindow(display),
                                sizehints.x,sizehints.y,
                                sizehints.width+MARGIN*2,
                                sizehints.height+MARGIN*2,
                                1,
                                xcolor[0],xcolor[0]); // colori

 {
  XTextProperty name;
  char *str="Z e d";
  char *icon="zed";

  if (XStringListToTextProperty(&str,1,&name)!=0)
  {
   XSetWMName(display,main_win,&name);
   XFree(name.value);
  }

  if (XStringListToTextProperty(&icon,1,&name)!=0)
  {
   XSetWMIconName(display,main_win,&name);
   XFree(name.value);
  }
 }

 #ifdef XPM_ICON
 if (xcolormode) // all color
 {
  Pixmap dum;
  int status;

  XpmAttributes xfig_icon_attr;

  xfig_icon_attr.valuemask=XpmReturnPixels|XpmCloseness;
  xfig_icon_attr.colormap=colormap;
  xfig_icon_attr.npixels=0;
  status=XpmCreatePixmapFromData(display,main_win,
           zedicoxpm,&icon_pixmap,&dum,&xfig_icon_attr);

 }
 #endif

 myclass.res_name="Zed";
 myclass.res_class="Editor";
 wmhints.input=True;
 wmhints.initial_state=NormalState;
 wmhints.icon_pixmap=icon_pixmap;
 wmhints.flags=InputHint|StateHint|IconPixmapHint;

 XSetWMProperties(display,main_win,NULL,NULL,0,0,
                  &sizehints,&wmhints,&myclass);

 XSelectInput(display,main_win,MW_EVENTS);

 zed_win=XCreateSimpleWindow(display,main_win,MARGIN/*sbar.width+1*/,MARGIN,
                               sizehints.width/*-sbar.width-1*/,
                               sizehints.height,0,
                               xcolor[0],xcolor[0]); // colori
 cursor=XCreateFontCursor(display,XC_xterm);
 XDefineCursor(display,zed_win,cursor);
 XSelectInput(display,zed_win,ZED_EVENTS);

 XMapWindow(display,zed_win);
 XMapWindow(display,main_win);
}

/***************************************************************************/

void resize_window(void)
{
 Window root;
 int x, y;
 static Bool first = True;
 int nw,nh;
 unsigned int width,height,border_width,depth;
 XEvent dummy;

 while (XCheckTypedWindowEvent(display,main_win,ConfigureNotify,&dummy));
 XGetGeometry(display,main_win,&root,&x,&y,&width,&height,&border_width,
              &depth);
 nw=(width-2*MARGIN)/MyWinInfo.fwidth;
 nh=(height-2*MARGIN)/MyWinInfo.fheight;

 if ((nw!=MyWinInfo.cwidth)||(nh!=MyWinInfo.cheight))
 {
  MyWinInfo.cheight=nh;
  MyWinInfo.cwidth=nw;
  MyWinInfo.pwidth=MyWinInfo.cwidth*MyWinInfo.fwidth;
  MyWinInfo.pheight=MyWinInfo.cheight*MyWinInfo.fheight;

  XResizeWindow(display,zed_win,MyWinInfo.pwidth,MyWinInfo.pheight);
  XClearWindow(display,zed_win);
  XSync(display,0);

  if (!first) goresize();
 }
 first=False;
}

/***************************************************************************/

void okmouse(void)
{
 if (crtdialog) { mouse.togox=-1; mouse.togoy=-1; }

 if (mouse.togox>-1 || mouse.togoy>-1)
 {
  int dx,dy;
  static int nx=-1,ny=-1;
  int ax,ay;

  if (mouse.togox<0) mouse.togox=0;
  if (mouse.togox>config.maxx-1) mouse.togox=config.maxx-1;
  if (mouse.togoy<0) mouse.togoy=0;
  if (mouse.togoy>config.maxy-1) mouse.togoy=config.maxy-1;

  if (mouse.win==1 && winn>0 && crtselez==0)
  {
   mouse.togoy=mouse.togoy>?win[cwin].myda;
   mouse.togoy=mouse.togoy<?win[cwin].mya;
  }

  dx=mouse.togox; dy=mouse.togoy;

  mouse.exec=-1;
  stopupdate=1;

  if (crtselez) { ax=-1; ay=crtselez->cy+crtselez->yda+1; }
  else { ax=win[cwin].edtr->askvidx(); ay=win[cwin].edtr->askvidy(); }

  if (mouse.togoy!=-1 && ay!=dy && ay!=ny)
  {
   ny=ay;

   if (crtselez)
   {
    if (ny>dy) mouse.exec=2; // freccia su;
    else
    if (ny<dy) mouse.exec=1; // freccia giu'
   }
   else
   {
    if (dy<win[cwin].myda) mouse.exec=19; // prev win
    else
    if (dy>win[cwin].mya) mouse.exec=18; // next win
    else
    if (ny>dy && ny!=win[cwin].myda+1) mouse.exec=2; // freccia su;
    else
    if (ny<dy) mouse.exec=1; // freccia giu'
   }
  }

  if (!crtselez && mouse.exec==-1 && mouse.togox!=-1 && ax!=dx && ax!=nx)
  {
   nx=ax;

   if (nx>dx) mouse.exec=132; // freccia sinistra
   else
   if (nx<dx) mouse.exec=131; // freccia destra
  }

  if (mouse.exec==-1 || (mouse.togox==-1 && mouse.togoy==-1))
  {
   stopupdate=0;
   if (crtselez)
    crtselez->setcur(1,crtselez->cy+1);
   else win[cwin].edtr->setcur();
   nx=-1; ny=-1;
   mouse.togox=-1; mouse.togoy=-1;
  }
 }
}

/***************************************************************************/
// prelevo gli eventi &c

unsigned int getxkey(void)
{
 XEvent event;
 unsigned int ret=0xfffffffe;
 static unsigned int nextkey=0xffffffff;

 if (nextkey!=0xfffffff)
 {
  ret=nextkey; nextkey=0xfffffff; return(ret);
 }

 if (mouse.togox>-1 || mouse.togoy>-1) { okmouse(); return(0xfffffffd); }

 { // isolo variabili locali
  fd_set fds;
  int ok=1;

  do
  {
   XFlush(display);

   if (!XPending(display))
   {
    int fdmax=fd_x;
    int i;

    FD_ZERO(&fds);
    FD_SET(fd_x,&fds);
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
     timeval tv;

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
     timeval tv;

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
    if (FD_ISSET(fd_x,&fds)) ok=0;
   } else ok=0;
  } while(ok);
 }
 if (dialof&512) return(0xffffffff);

 XNextEvent(display,&event);

 switch(event.type)
 {
  case KeyPress:
  {
   int sta;
   KeySym keysym;
   char kbuf[256];
   static XComposeStatus compose={0,0};
   sele.time=event.xkey.time;

#if ((ShiftMask  ==(1<<0)) && (LockMask   ==(1<<1)) && \
     (ControlMask==(1<<2)) && (Mod1Mask   ==(1<<3)) && \
     (Mod2Mask   ==(1<<4)) && (Mod3Mask   ==(1<<5)) && \
     (Mod4Mask   ==(1<<6)) && (Mod5Mask   ==(1<<7)))

   sta=event.xkey.state&0xff;

#else

   sta=((event.xkey.state&ShiftMask  )?(1<<0):0) |
       ((event.xkey.state&LockMask   )?(1<<1):0) |
       ((event.xkey.state&ControlMask)?(1<<2):0) |
       ((event.xkey.state&Mod1Mask   )?(1<<3):0) |
       ((event.xkey.state&Mod2Mask   )?(1<<4):0) |
       ((event.xkey.state&Mod3Mask   )?(1<<5):0) |
       ((event.xkey.state&Mod4Mask   )?(1<<6):0) |
       ((event.xkey.state&Mod5Mask   )?(1<<7):0);

#endif

   XLookupString(&event.xkey,kbuf,256,&keysym,&compose);

#if ((XK_Shift_L   == 0xFFE1) && (XK_Shift_R   == 0xFFE2) && \
     (XK_Control_L == 0xFFE3) && (XK_Control_R == 0xFFE4) && \
     (XK_Caps_Lock == 0xFFE5) && (XK_Shift_Lock== 0xFFE6) && \
     (XK_Meta_L    == 0xFFE7) && (XK_Meta_R    == 0xFFE8) && \
     (XK_Alt_L     == 0xFFE9) && (XK_Alt_R     == 0xFFEA) && \
     (XK_Super_L   == 0xFFEB) && (XK_Super_R   == 0xFFEC) && \
     (XK_Hyper_L   == 0xFFED) && (XK_Hyper_R   == 0xFFEE))

   if ((keysym<0xffe1 || keysym>0xffee) && keysym!=XK_Mode_switch)

#else

  if (keysym!=XK_Shift_L && keysym!=XK_Shift_R &&
      keysym!=XK_Control_L && keysym!=XK_Control_R &&
      keysym!=XK_Caps_Lock && keysym!=XK_Shift_Lock &&
      keysym!=XK_Meta_L && keysym!=XK_Meta_R &&
      keysym!=XK_Alt_L && keysym!=XK_Alt_R &&
      keysym!=XK_Super_L && keysym!=XK_Super_R &&
      keysym!=XK_Hyper_L && keysym!=XK_Hyper_R && keysym!=XK_Mode_switch)

#endif

   {
    if (keysym<255 && ((sta&(0xff-0x3))==0))
     ret=keysym; else ret=(sta<<24)|keysym;
   }

  } break;
  case ClientMessage:
  {
   if (event.xclient.format==32 &&
       (int)event.xclient.data.l[0]==(int)wm_del_win)
    mouse.exec=15; ret=0xfffffffd; // w_qquita !
  //       clean_exit(0);
  } break;
  case MappingNotify:
  {
   XRefreshKeyboardMapping(&event.xmapping);
  } break;
  case GraphicsExpose:
  case Expose:
  {
   zedmapped=1;
   if (event.xany.window==zed_win)
   {
    int xda,xa,yda,ya,yy;

    xda=event.xexpose.x/MyWinInfo.fwidth;
    xa=xda+event.xexpose.width/MyWinInfo.fwidth+1;
    yda=event.xexpose.y/MyWinInfo.fheight;
    ya=yda+event.xexpose.height/MyWinInfo.fheight+1;

    xda=0>?xda; xa=(config.maxx-1)<?xa;
    yda=0>?yda; ya=(config.maxy)<?ya;

    for (yy=yda; yy<=ya; yy++)
    {
     if (astat[yy].modif)
     {
      astat[yy].da=astat[yy].da<?xda;
      astat[yy].a =astat[yy].a >?xa;
     }
     else { astat[yy].da=xda; astat[yy].a=xa; }
     astat[yy].modif=2;
    }
    if (event.xexpose.count==0) refresh();
    break;
   }
  } break;
  case VisibilityNotify:
  {
   if (event.xvisibility.state==VisibilityUnobscured)
    zedexpose=1; else zedexpose=0;
  } break;
  case FocusIn:  zedfocus=1; vsetcur(0,0,1); break;
  case FocusOut: zedfocus=0; vsetcur(0,0,1); break;
  case ConfigureNotify:
  {
   resize_window();
  //    size_set=1;
  } break;
  case SelectionClear:
  {
   if (sele.str) delete sele.str; sele.len=0; sele.pos=0; sele.str=0;
  } break;
  case SelectionNotify:
  {
   clip.edtr->importclip(event.xselection.requestor,
   event.xselection.property);
   ret=0xfffffffc;
  } break;
  case SelectionRequest:
  {
   XEvent myevent;
   static Atom xa_targets=None;

   if (xa_targets==None)
    xa_targets=XInternAtom(display,"TARGETS",False);

   myevent.xselection.type=SelectionNotify;
   myevent.xselection.property=None;
   myevent.xselection.display=event.xselectionrequest.display;
   myevent.xselection.requestor=event.xselectionrequest.requestor;
   myevent.xselection.selection=event.xselectionrequest.selection;
   myevent.xselection.target=event.xselectionrequest.target;
   myevent.xselection.time=event.xselectionrequest.time;

   if (sele.pos)
   {
    if (event.xselectionrequest.target==xa_targets)
    {
     unsigned int target_list[2];

     target_list[0]=(unsigned int)xa_targets;
     target_list[1]=(unsigned int)XA_STRING;

     XChangeProperty(display,event.xselectionrequest.requestor,
                        event.xselectionrequest.property,xa_targets,
                        8*sizeof(target_list[0]), PropModeReplace,
                        (unsigned char *)target_list,
                        sizeof(target_list)/sizeof(target_list[0]));
     myevent.xselection.property=event.xselectionrequest.property;
    }


    if (event.xselectionrequest.target==XA_STRING)
    {
     XChangeProperty(display,event.xselectionrequest.requestor,
                     event.xselectionrequest.property,XA_STRING,8,
                     PropModeReplace,
                     (unsigned char *)sele.str,sele.pos);
     myevent.xselection.property=event.xselectionrequest.property;
    }
   }
   XSendEvent(display,event.xselectionrequest.requestor,False,0,&myevent);
  } break;

  case ButtonPress:
  case MotionNotify:
  case ButtonRelease:
  {
   int build=0x00111000;
   int sta;

   sele.time=event.xbutton.time;

   if (event.xany.window!=zed_win) break;

   if (event.type!=MotionNotify)
   {
    if (event.xbutton.button==Button1) build|=1;
    else if (event.xbutton.button==Button2) build|=2;
    else if (event.xbutton.button==Button3) build|=3;
    else if (event.xbutton.button==Button4) build|=4;
    else if (event.xbutton.button==Button5) build|=5;
    else break;
   }
   else
   {
    if (event.xmotion.state&Button1Mask) build|=1;
    else if (event.xmotion.state&Button2Mask) build|=2;
    else if (event.xmotion.state&Button3Mask) build|=3;
    else if (event.xmotion.state&Button4Mask) build|=4;
    else if (event.xmotion.state&Button5Mask) build|=5;
    else break;
   }

#if ((ShiftMask  ==(1<<0)) && (LockMask   ==(1<<1)) && \
     (ControlMask==(1<<2)) && (Mod1Mask   ==(1<<3)) && \
     (Mod2Mask   ==(1<<4)) && (Mod3Mask   ==(1<<5)) && \
     (Mod4Mask   ==(1<<6)) && (Mod5Mask   ==(1<<7)))

   if (event.type!=MotionNotify) sta=event.xbutton.state&0xff;
   else                          sta=event.xmotion.state&0xff;

#else

   if (event.type!=MotionNotify)
    sta=((event.xbutton.state&ShiftMask  )?(1<<0):0) |
        ((event.xbutton.state&LockMask   )?(1<<1):0) |
        ((event.xbutton.state&ControlMask)?(1<<2):0) |
        ((event.xbutton.state&Mod1Mask   )?(1<<3):0) |
        ((event.xbutton.state&Mod2Mask   )?(1<<4):0) |
        ((event.xbutton.state&Mod3Mask   )?(1<<5):0) |
        ((event.xbutton.state&Mod4Mask   )?(1<<6):0) |
        ((event.xbutton.state&Mod5Mask   )?(1<<7):0);
   else
    sta=((event.xmotion.state&ShiftMask  )?(1<<0):0) |
        ((event.xmotion.state&LockMask   )?(1<<1):0) |
        ((event.xmotion.state&ControlMask)?(1<<2):0) |
        ((event.xmotion.state&Mod1Mask   )?(1<<3):0) |
        ((event.xmotion.state&Mod2Mask   )?(1<<4):0) |
        ((event.xmotion.state&Mod3Mask   )?(1<<5):0) |
        ((event.xmotion.state&Mod4Mask   )?(1<<6):0) |
        ((event.xmotion.state&Mod5Mask   )?(1<<7):0);
#endif

   build|=sta<<24;

   mouse.px=mouse.x; mouse.py=mouse.y;
   if (event.type!=MotionNotify)
   {
    mouse.x=event.xbutton.x/MyWinInfo.fwidth;
    mouse.y=event.xbutton.y/MyWinInfo.fheight;
   }
   else
   {
    mouse.x=event.xmotion.x/MyWinInfo.fwidth;
    mouse.y=event.xmotion.y/MyWinInfo.fheight;
   }
   if (mouse.x<0) mouse.x=0;
   if (mouse.y<0) mouse.y=0;
   if (mouse.x>config.maxx-1) mouse.x=config.maxx-1;
   if (mouse.y>config.maxy-1) mouse.y=config.maxy-1;

   switch(event.type)
   {
    case ButtonPress: mouse.mot=0; mouse.rx=mouse.x; mouse.ry=mouse.y; break;
    case MotionNotify:
    {
     if (mouse.x==mouse.px && mouse.y==mouse.py) { build=0; break; }
     if (mouse.mot==0)
     {
      mouse.mot=1; build|=0x10;
      mouse.x=mouse.rx; mouse.y=mouse.ry;
     }
     else build|=0x20;
    } break;
    case ButtonRelease:
    {
     if (mouse.mot==0) build|=0x50; else
     { nextkey=build|0x40; build|=0x30; }
    } break;
   }

   if (build==0) break;

   if (crtdialog)
   {
    int pos=0x900;
    if ((mouse.y<=crtdialog->retpos()+1 && mouse.y>=crtdialog->retpos()-1)
       && mouse.x<80)
     pos=0x800;

    build|=pos;
   }
   else
   if (crtselez)
   {
    int pos=0;
    if (mouse.y>crtselez->ya || mouse.y<crtselez->yda) pos=0x700;
    else if (mouse.x<crtselez->xda) pos=0x500;
    else if (mouse.x>crtselez->xa) pos=0x600;
    else pos=0x400;

    build|=pos;
   }
   else
   if (winn>0)
   {
    if (mouse.y==win[cwin].myda)
    { if (mouse.x<45) build|=0x200; else build|=0x300; }
    else
    if (mouse.y<win[cwin].mya && mouse.y>win[cwin].myda) build|=0x100;
   }

   if (nextkey!=0xffffffff) nextkey|=(build&0x0f00);

   ret=build;
  } break;

 }
 return(ret);
}

/***************************************************************************/

