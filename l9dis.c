#define PRGNAME "L9Dis"
#define PRGVERSION "0.4321 BETA"

/*
   #define MESSNUMDEBUG
   #define DICTDEBUG
   #define CHARDEBUG
   #define DEBUG
 */

/*
   l9dis.c

   - disassembles Level 9 datafile

   Author: Paul David Doherty <h0142kdd@rz.hu-berlin.de>
   Archive: https://ifarchive.org/indexes/if-archive/level9/tools/
   Extensions: https://github.com/balyuf/l9dis.git
   Integrated parts of: https://github.com/DavidKinder/Level9.git
   Inspiration: https://github.com/ajgbarnes/level9.git

   v0.0:    29 Nov 1997
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#ifdef __TURBOC__
#include <io.h>
#endif
#include <ctype.h>

typedef unsigned char type8;
typedef unsigned short type16;
typedef unsigned long int type32;

/* other global stuff */
#define TRUE 1
#define FALSE 0

FILE *fdi;
type8 *infiledata;
type32 infilelength;
type16 l9length;
type8 l9check;

type16 pc;
type8 code;
type8 jumped = FALSE;
type16 maxaddr = 0;
type8 highvar = 0;
type16 highword = 0xffff;
type8 jumptablesize[10] = {0,}, newjumptablesize = 0, jumptablenmbr;
type16 jumptable[10] = {0,};
type8 pass = 0, runjump = 0;

/* header values */
type16 startmd, startmdV2, endmd, endmdV2, defdict, enddict, dictdata, dictdatalen;
type16 wordtable, unknown10, absdatablock;
type16 list0, list1, list2, list3, list4, list5;
type16 list6, list7, list8, list9, acodeptr, acodeend;

/* V2 support */
type8 isV2 = FALSE;

/* V1 support */
struct L9V1GameInfo
{
        type8 dictVal1, dictVal2;
        int dictStart, L9Ptrs[5], absData, msgStart, msgLen, acode;
};
struct L9V1GameInfo L9V1Games[] =
{
        { 0x1a,0x24,301, { 0x0000,-0x004b, 0x0080,-0x002b, 0x00d0 },0x03b0, 0x0f80,0x4857,0x0760 }, /* Colossal Adventure */
        { 0x20,0x3b,283, {-0x0583, 0x0000,-0x0508,-0x04e0, 0x0000 },0x0800, 0x1000,0x39d1,0x04c8 }, /* Adventure Quest */
        { 0x14,0xff,153, {-0x00d6, 0x0000, 0x0000, 0x0000, 0x0000 },0x0a20, 0x16bf,0x420d,0x0740 }, /* Dungeon Adventure */
        { 0x15,0x5d,252, {-0x3e70, 0x0000,-0x3d30,-0x3ca0, 0x0100 },0x4120,-0x3b9d,0x3988,0x4a00 }, /* Lords of Time */
        { 0x15,0x6c,284, {-0x00f0, 0x0000,-0x0050,-0x0050,-0x0050 },0x0300, 0x1930,0x3c17,0x0a10 }, /* Snowball */
};
int L9V1Game = -1;
type16 L9Pointers[12];

/* arbitrary V3/V4 distinction based on Archers/Adrian Mole */
#define V3LIMIT 0x8500

/* leave the program due to error */
void
ex (char *error)
{
  fprintf (stderr, PRGNAME ": %s\n", error);
  exit (1);
}

/* called with too few/too many parameters */
void
usage (void)
{
  fprintf (stderr,
	   PRGNAME " version " PRGVERSION ":\n");
  fprintf (stderr,
	   "Disassembles Level 9 datafile.\n");
  fprintf (stderr, "Source code available on request.\n");
  fprintf (stderr,
	   "(c) 1997 by Paul David Doherty <h0142kdd@rz.hu-berlin.de>\n\n");
  fprintf (stderr, "Usage: " PRGNAME " infile\n");
  exit (1);
}


/* calculate word starting at byte position */
type16
calcwordLE (type16 i)
{
/* little-endian (header, A-code) */
  type16 calc;
  calc = infiledata[i];
  calc += 256 * (infiledata[i + 1]);
  return (calc);
}

type16
calcwordBE (type16 i)
{
/* big-endian (word references) */
  type16 calc;
  calc = 256 * infiledata[i];
  calc += (infiledata[i + 1]);
  return (calc);
}

void ScanV1(void)
{
  type32 i;
  int dictOff1, dictOff2;
  type8 dictVal1 = 0xff, dictVal2 = 0xff;

  /* V1 dictionary detection from L9Cut by Paul David Doherty */
  for (i=0;i<infilelength-20;i++)
  {
    if (infiledata[i]=='A')
    {
      if (infiledata[i+1]=='T' && infiledata[i+2]=='T' && infiledata[i+3]=='A' && infiledata[i+4]=='C' && infiledata[i+5]==0xcb)
      {
        dictOff1 = i;
        dictVal1 = infiledata[dictOff1+6];
        break;
      }
    }
  }
  for (i=dictOff1;i<infilelength-20;i++)
  {
    if (infiledata[i]=='B')
    {
      if (infiledata[i+1]=='U' && infiledata[i+2]=='N' && infiledata[i+3]=='C' && infiledata[i+4] == 0xc8)
      {
        dictOff2 = i;
        dictVal2 = infiledata[dictOff2+5];
        break;
      }
    }
  }
  L9V1Game = -1;
  if (dictVal1 != 0xff || dictVal2 != 0xff)
  {
    for (i = 0; i < sizeof L9V1Games / sizeof L9V1Games[0]; i++)
    {
      if ((L9V1Games[i].dictVal1 == dictVal1) && (L9V1Games[i].dictVal2 == dictVal2))
      {
        L9V1Game = i;
        dictdata = dictOff1-L9V1Games[i].dictStart;
      }
    }
  }
  if (L9V1Game != -1) {
    l9length = infilelength - 2;
    acodeptr = dictdata + L9V1Games[L9V1Game].acode;
    for (i = 0; i < 5; i++) {
      int off = L9V1Games[L9V1Game].L9Ptrs[i];
      if (off < 0)
        L9Pointers[i + 2] = acodeptr + off;
      else
        L9Pointers[i + 2] = 0x8000 + off;
    }
  }
}

void
docheck (void)
{
  type16 j;
  type8 checksum;

  l9length = calcwordLE (0x1c); /* v2 */
  if (l9length > 0 && l9length < infilelength) {
    checksum = 0;
    for (j = 0x20; j <= l9length; j++)
      checksum += infiledata[j];
    if (checksum == infiledata[0x1e])
      { isV2 = TRUE; return; }
  }

  l9length = calcwordLE (0); /* v3-4 */
  if (l9length > 0 && l9length < infilelength) {
    checksum = 0;
    for (j = 0; j <= l9length; j++)
      checksum += infiledata[j];
    if (checksum == 0) return;
  }

  ScanV1(); /* v1 */
  if (L9V1Game != -1)
    { isV2 = TRUE; return; }

  ex ("corrupt datafile");
}


#define SEP 0xf0

/* used for Champion of the Raj */
#define AAUML 142
#define AUML 132
#define OOUML 153
#define OUML 148
#define UUUML 154
#define UUML 129
#define SZLIG 225

type8 new_mess = TRUE;
type8 end_mess = FALSE;
type8 count_in_line;

void
prt_space (void)
{
/* 65 for a maximum line length of 76...78 */
  if (count_in_line > 65)
    {
      fprintf (stdout, "\n       ");
      count_in_line = 8;
    }
  else
    {
      fprintf (stdout, " ");
      count_in_line++;
    }
}

type8 last_char, last_real_char;

void
prt_char (type8 c)
{
#ifdef CHARDEBUG
  fprintf (stdout, "[%02x]", c);
#endif

  if ((new_mess == TRUE))
    {
      count_in_line = 8;
      last_char = ',';
      last_real_char = ' ';
      new_mess = FALSE;
    }

  if ((c & 128) && (c != SEP))
    {
      c &= 0x7f;
      last_char = c;
      fprintf (stdout, "***strangechar:");
    }
  else if ((c != ' ') && (c != SEP) && (c != 0x0d) && (c != 0x7e) &&
	   (c < '\"' || c > '+'))
    {
      if (last_char == '!' || last_char == '?' ||
	  (last_char == '.' /* NOT: &&last_real_char==' ' */ ) ||
	  last_real_char == 0x0d)
	c = toupper (c);

      last_char = c;
    }

/* umlaute */
  if (c == 0x1a)
    c = UUUML;
  if (c == 0x04)
    c = AUML;
  if (c == 0x14)
    c = OUML;
  if (c == 0x01)
    c = UUML;
  if (c == 0x11)
    c = SZLIG;

  if (c == ' ')
    prt_space ();

  else if ((c == SEP))
    {
      if ((last_real_char != ' ') && (last_real_char != '.') &&
	  (last_real_char != '\"') && (last_real_char != '\'') &&
	  (last_real_char != '(') && (last_real_char != ':') &&
	  (last_real_char != 0x0d) && (last_real_char != UUUML) &&
	  (last_real_char != AUML) && (last_real_char != OUML) &&
	  (last_real_char != UUML) && (last_real_char != SZLIG))
	prt_space ();
    }

  else if (c == 0x0d)
    {
      fprintf (stdout, "\n       ");
      count_in_line = 8;
    }

  else
    {
      fprintf (stdout, "%c", c);
      count_in_line++;
    }

  if (c != SEP)
    last_real_char = c;
  if (c == 0x7e)
    fprintf (stdout, "***[7E]");
}

/* disassembly functions */

void
prtaddr (void)
{
  type16 realaddr;
  type16 temp1;

  if (code & 0x20)
    {
      temp1 = (type16) infiledata[pc];
      if (temp1 < 0x80)
	{
	  realaddr = pc - acodeptr + temp1;
	}
      else
	/* minus if >= 0x80 */
	{
	  realaddr = pc - acodeptr - 0x0100 + temp1;
	}
      if (pass == 4)
	{
	  fprintf (stdout, "%05u ", realaddr);	/* SADDR */
	  fprintf (stdout, "[#%04x]", realaddr);
	}
      pc++;
    }
  else
    {
      realaddr = calcwordLE (pc);
      if (pass == 4)
	{
	  fprintf (stdout, "%05u ", realaddr);	/* LADDR */
	  fprintf (stdout, "[#%04x]", realaddr);
	}
      pc = pc + 2;
    }

  if (pass == 3)
    {
      if ((realaddr > jumptable[jumptablenmbr]) && (realaddr < (jumptable[jumptablenmbr] + 2 * jumptablesize[jumptablenmbr])))
	jumptablesize[jumptablenmbr] = (type8) ((realaddr - jumptable[jumptablenmbr]) / 2);
    }

  if (((type32) realaddr + (type32) acodeptr) > (type32) acodeend)
    {
      if ((pass == 2) && (runjump == 1) && (newjumptablesize == 0))
	newjumptablesize = (type8) ((pc - 2 - acodeptr - jumptable[jumptablenmbr]) / 2);
      if (pass == 4)
	fprintf (stdout, "*** JUMP TOO HIGH ***");
    }

  if (((type32) realaddr + (type32) acodeptr) <= (type32) acodeend &&
      ((type32) realaddr + (type32) acodeptr) > (type32) maxaddr)
     maxaddr = realaddr + acodeptr;
}


type8
getbyte (void)
{
  return (infiledata[pc++]);
}

void
prtbyte (void)
{
  type8 temp8 = getbyte ();
  if (pass == 4)
    fprintf (stdout, "#%02x", temp8);
}

type16
getword (void)
{
  type16 temp;
  temp = calcwordLE (pc);
  pc = pc + 2;
  return (temp);
}
void
prtword (void)
{
  type16 temp16 = getword ();
  if (pass == 4)
    fprintf (stdout, "WORD#%04x", temp16);
}

type16
getcon (void)
{
  type16 contemp;
  if (code & 0x40)
    contemp = (type16) infiledata[pc++];
  else
    {
      contemp = calcwordLE (pc);
      pc = pc + 2;
    }
  return contemp;
}

void
prtcon (void)
{
  type8 temp8;

  if (code & 0x40)
    {
      temp8 = infiledata[pc++];
      if (pass == 4)
	fprintf (stdout, "#%02x", temp8);
    }
  else
    {
      if (pass == 4)
	fprintf (stdout, "#%04x", calcwordLE (pc));
      pc = pc + 2;
    }
}

void
prtvar (void)
{
  type8 variab = infiledata[pc++];
  if (pass == 4)
    {
      fprintf (stdout, "G%03d", variab);
      if (variab > highvar)
	highvar = variab;
    }
}


void
prtline (void)
{
  fprintf (stdout, "-----------------------------------------\n");
}


/* instruction decoder */

void
do_instr (void)
{
  type8 temp8, temp8a;
  type16 temp16;

  if (code >= 0x80)
    {
/* listhandler */
      if (((code & 0x1f) == 0x00) ||
	  ((code & 0x1f) > 0x09 /* was 0x0a in Glen's */ ))
	{
	  if (pass == 4)
	    fprintf (stdout, "*** illegal list %d: ", code & 0x1f);
	}

      if ((code >= 0xe0) && (code < 0xff))	/* listvv */
	{
	  if (pass == 4)
	    fprintf (stdout, "LIST#%02d [", code & 0x1f);
	  prtvar ();
	  if (pass == 4)
	    fprintf (stdout, "] = ");
	  prtvar ();
	}
      else if ((code >= 0xa0) && (code < 0xc0))		/* listv1v */
	{
	  pc++;
	  prtvar ();
	  pc = pc - 2;
	  if (pass == 4)
	    fprintf (stdout, " = LIST#%02d [", code & 0x1f);
	  prtvar ();
	  pc++;
	  if (pass == 4)
	    fprintf (stdout, "]");
	}
      else if ((code >= 0xc0) && (code < 0xe0))		/* listv1c */
	{
	  pc++;
	  prtvar ();
	  pc = pc - 2;
	  if (pass == 4)
	    fprintf (stdout, " = LIST#%02d [", code & 0x1f);
	  prtbyte ();
	  pc++;
	  if (pass == 4)
	    fprintf (stdout, "]");
	}
      else if ((code >= 0x80) && (code < 0xa0))
	{
	  if (pass == 4)
	    fprintf (stdout, "LIST#%02d [", code & 0x1f);
	  prtbyte ();
	  if (pass == 4)
	    fprintf (stdout, "] = ");
	  prtvar ();
	}
    }
  else
    {
/* normal opcodes */
      switch (code & 0x1f)
	{
	case 0:
	  if (pass == 4)
	    fprintf (stdout, "GOTO ");
	  prtaddr ();
	  jumped = TRUE;
	  break;

	case 1:
	  if (pass == 4)
	    fprintf (stdout, "GOSUB ");
	  prtaddr ();
	  break;

	case 2:
	  if (pass == 4)
	    fprintf (stdout, "RETURN");
	  jumped = TRUE;
	  break;

	case 3:
	  if (pass == 4)
	    fprintf (stdout, "PRINTNUMBER ");
	  prtvar ();
	  break;

	case 4:		/* messagev */
	  if (pass == 4)
	    fprintf (stdout, "PRINTMESSAGE (");
	  prtvar ();
	  if (pass == 4)
	    fprintf (stdout, ")");
	  break;

	case 5:
	  temp16 = getcon ();
	  if (pass == 4)
	    fprintf (stdout, "PRINTMESSAGE (M%04d)", temp16);
	  break;

	case 6:		/* function */
	  temp8 = getbyte ();
	  switch (temp8)
	    {
	    case 1:
	      if (pass == 4)
		fprintf (stdout, "CALLDRIVER");
	      break;
	    case 2:
	      if (pass == 4)
		fprintf (stdout, "RANDOM ");
	      prtvar ();
	      break;
	    case 3:
	      if (pass == 4)
		fprintf (stdout, "SAVE");
	      break;
	    case 4:
	      if (pass == 4)
		fprintf (stdout, "RESTORE");
	      break;
	    case 5:
	      if (pass == 4)
		fprintf (stdout, "CLEARWORKSPACE");
	      break;
	    case 6:
	      if (pass == 4)
		fprintf (stdout, "CLEARSTACK");
	      break;
	    case 250:
	      if (pass == 4)
		fprintf (stdout, "PRINTSTRING \"");
	      while (infiledata[pc] != 0)
		{
		  new_mess = TRUE;
		  temp8a = infiledata[pc++];
		  if (pass == 4)
		    prt_char (temp8a);
		}
	      if (pass == 4)
		fprintf (stdout, "\"");
	      pc++;
	      break;
	    default:
	      if (pass == 4)
		fprintf (stdout, "*** ILLEGAL FUNCTION %d ***", temp8);
	      break;
	    }
	  break;

	case 7:
	  if (pass == 4)
	    fprintf (stdout, "INPUT ");
	  prtbyte ();
	  if (pass == 4)
	    fprintf (stdout, " ");
	  prtbyte ();
	  if (pass == 4)
	    fprintf (stdout, " ");
	  prtbyte ();
	  if (pass == 4)
	    fprintf (stdout, " ");
	  prtbyte ();
	  if (pass == 4)
	    fprintf (stdout, " ");
	  break;

	case 8:		/* VARCON */
	  temp16 = getcon ();
	  prtvar ();
	  if (pass == 4)
	    {
	      if (temp16 > 0x00ff)
		fprintf (stdout, " = #%04x", temp16);
	      else
		fprintf (stdout, " = #%02x", temp16);
	    }
	  break;

	case 9:		/* VARVAR */
	  pc++;
	  prtvar ();
	  if (pass == 4)
	    fprintf (stdout, " = ");
	  pc = pc - 2;
	  prtvar ();
	  pc++;
	  break;

	case 0x0a:		/* ADD */
	  pc++;
	  prtvar ();
	  if (pass == 4)
	    fprintf (stdout, " += ");
	  pc = pc - 2;
	  prtvar ();
	  pc++;
	  break;

	case 0x0b:		/* SUB */
	  pc++;
	  prtvar ();
	  if (pass == 4)
	    fprintf (stdout, " -= ");
	  pc = pc - 2;
	  prtvar ();
	  pc++;
	  break;

/* C & D illegal */

	case 0x0e:
	  if (pass == 4)
	    fprintf (stdout, "JUMPTABLE ");
	  temp16 = getword ();
	  if (pass == 4)
	    {
	      fprintf (stdout, "%05u ", temp16);
	      fprintf (stdout, "[#%04x] ", temp16);
	    }
	  if ((jumptable[jumptablenmbr] < temp16))
            {
              if (jumptable[jumptablenmbr] != 0 && jumptablenmbr < 9)
                jumptablenmbr++;
              if (pass == 1)
                {
                  jumptable[jumptablenmbr] = temp16;
                  jumptablesize[jumptablenmbr] = 255;
                }
            }
	  if (pass == 4)
	    fprintf (stdout, " <- ");
	  prtvar ();
	  if (((type32) temp16 + (type32) acodeptr) > (type32) acodeend)
	    if (pass == 4)
	      fprintf (stdout, " *** JT TOO HIGH ***");
	  jumped = TRUE;
	  break;

	case 0x0f:
	  if (pass == 4)
	    fprintf (stdout, "EXIT ");
	  prtvar ();
	  if (pass == 4)
	    fprintf (stdout, " ");
	  prtvar ();
	  if (pass == 4)
	    fprintf (stdout, " ");
	  prtvar ();
	  if (pass == 4)
	    fprintf (stdout, " ");
	  prtvar ();
	  break;

	case 0x10:
	  if (pass == 4)
	    fprintf (stdout, "IF ");	/* ifeqvt */
	  prtvar ();
	  if (pass == 4)
	    fprintf (stdout, " == ");
	  prtvar ();
	  if (pass == 4)
	    fprintf (stdout, " GOTO ");
	  prtaddr ();
	  break;

	case 0x11:
	  if (pass == 4)
	    fprintf (stdout, "IF ");	/* ifnevt */
	  prtvar ();
	  if (pass == 4)
	    fprintf (stdout, " != ");
	  prtvar ();
	  if (pass == 4)
	    fprintf (stdout, " GOTO ");
	  prtaddr ();
	  break;

	case 0x12:
	  if (pass == 4)
	    fprintf (stdout, "IF ");	/* ifltvt */
	  prtvar ();
	  if (pass == 4)
	    fprintf (stdout, " < ");
	  prtvar ();
	  if (pass == 4)
	    fprintf (stdout, " GOTO ");
	  prtaddr ();
	  break;

	case 0x13:
	  if (pass == 4)
	    fprintf (stdout, "IF ");	/* ifgtvt */
	  prtvar ();
	  if (pass == 4)
	    fprintf (stdout, " > ");
	  prtvar ();
	  if (pass == 4)
	    fprintf (stdout, " GOTO ");
	  prtaddr ();
	  break;

	case 0x14:
	  if (pass == 4)
	    fprintf (stdout, "SCREEN ");
	  temp8 = getbyte ();
	  if (temp8 == 0)
	    {
	      if (pass == 4)
		fprintf (stdout, "TEXTMODE");
	    }
	  else if (temp8 == 1)
	    {
	      if (pass == 4)
		fprintf (stdout, "GRAPHMODE  ");
	      prtbyte ();
	    }
	  else
	    {
	      if (pass == 4)
		fprintf (stdout, "  ***BAD SCREENMODE: %02d ", temp8);
	    }
	  break;

	case 0x15:
	  if (pass == 4)
	    fprintf (stdout, "CLEARTG ");
	  temp8 = getbyte ();
	  if (pass == 4)
	    {
	      if (temp8 == 0)
		fprintf (stdout, "TEXTMODE");
	      else if (temp8 == 1)
		fprintf (stdout, "GRAPHMODE");
	      else
		fprintf (stdout, "  ***BAD SCREENMODE: %02d ", temp8);
	    }
	  break;

	case 0x16:
	  if (pass == 4)
	    fprintf (stdout, "PICTURE ");
	  prtvar ();
	  break;

	case 0x17:
	  if (pass == 4)
	    fprintf (stdout, "GETNEXTOBJECT ");
	  prtvar ();
	  if (pass == 4)
	    fprintf (stdout, " ");
	  prtvar ();
	  if (pass == 4)
	    fprintf (stdout, " ");
	  prtvar ();
	  if (pass == 4)
	    fprintf (stdout, " ");
	  prtvar ();
	  if (pass == 4)
	    fprintf (stdout, " ");
	  prtvar ();
	  if (pass == 4)
	    fprintf (stdout, " ");
	  prtvar ();
	  if (pass == 4)
	    fprintf (stdout, " ");
	  break;

	case 0x18:
	  if (pass == 4)
	    fprintf (stdout, "IF ");	/* ifeqct */
	  prtvar ();
	  if (pass == 4)
	    fprintf (stdout, " == ");
	  prtcon ();
	  if (pass == 4)
	    fprintf (stdout, " GOTO ");
	  prtaddr ();
	  break;

	case 0x19:
	  if (pass == 4)
	    fprintf (stdout, "IF ");	/* ifnect */
	  prtvar ();
	  if (pass == 4)
	    fprintf (stdout, " != ");
	  prtcon ();
	  if (pass == 4)
	    fprintf (stdout, " GOTO ");
	  prtaddr ();
	  break;

	case 0x1a:
	  if (pass == 4)
	    fprintf (stdout, "IF ");	/* ifltct */
	  prtvar ();
	  if (pass == 4)
	    fprintf (stdout, " < ");
	  prtcon ();
	  if (pass == 4)
	    fprintf (stdout, " GOTO ");
	  prtaddr ();
	  break;

	case 0x1b:
	  if (pass == 4)
	    fprintf (stdout, "IF ");	/* ifgtct */
	  prtvar ();
	  if (pass == 4)
	    fprintf (stdout, " > ");
	  prtcon ();
	  if (pass == 4)
	    fprintf (stdout, " GOTO ");
	  prtaddr ();
	  break;

	case 0x1c:
	  if (pass == 4)
	    fprintf (stdout, "PRINTINPUT");
	  break;

	default:
	  if (pass == 4)
	    fprintf (stdout, "***ILLEGAL***");
	  break;
	}
    }

  if (pass == 4)
    {
      fprintf (stdout, "\n");
      if (jumped == TRUE)
	prtline ();
    }
}

void
do_disassembly (void)
{
  type16 i;

/*
   fprintf (stdout, "Running pass %d...\n", pass);
   fprintf (stdout, "Jumptable from %04x ", jumptable);
   fprintf (stdout, "to %04x, ", jumptable + (2 * jumptablesize) - 1);
   fprintf (stdout, "size %d\n", jumptablesize);
 */
  if (pass == 4)
    {
      fprintf (stdout, "\n");
      prtline ();
      fprintf (stdout, "A-code disassembly:\n");
      prtline ();
    }

  jumptablenmbr = 0;
  pc = acodeptr;
  while (pc < acodeend && !(jumped && pc > maxaddr))
    {
      if ((jumptable[jumptablenmbr] != 0) && ((acodeptr + jumptable[jumptablenmbr]) == pc))
	{
	  runjump = 1;

	  if (pass == 4)
	    fprintf (stdout, "JUMPTABLE:\n");
	  for (i = 0; i < jumptablesize[jumptablenmbr]; i++)
	    {
	      if (pass == 4)
		{
		  fprintf (stdout, " JT #%02x : ", i);
		  fprintf (stdout, "%05u ", (pc - acodeptr));
		  fprintf (stdout, "[#%04x] : GOTO ", (pc - acodeptr));
		}
	      code = 1;
	      prtaddr ();
	      if (pass == 4)
		fprintf (stdout, "\n");
	    }
	  if (pass == 4)
	    prtline ();
	  runjump = 0;
	  if (pass == 2)
	    jumptablesize[jumptablenmbr] = newjumptablesize;
	}
      code = infiledata[pc];
      jumped = FALSE;

      if (pc > acodeend)
	break;
      else
	{
	  if (pass == 4)
	    {
	      fprintf (stdout, "%05u ", (pc - acodeptr));
	      fprintf (stdout, "[#%04x] : ", (pc - acodeptr));
	      /*  fprintf (stdout, "$#%02x$ ", code);         */
	    }
	  pc++;
	  do_instr ();
	}
    }
  if (pass == 4)
    {
      fprintf (stdout, "End of A-code\n\n");
      fprintf (stdout, "Highest variable: G%03d\n", highvar);
    }
}

/*
   dictionary handling
 */

char unpackbuf[8];
type16 dictptr;
type8 unpackcount = 8;

char
getdictionarycode (void)
{
  if (unpackcount != 8)
    return unpackbuf[unpackcount++];
  else
    {
      type8 d1, d2;

      /* unpackbytes */
      d1 = infiledata[dictptr++];
      unpackbuf[0] = d1 >> 3;
      d2 = infiledata[dictptr++];
      unpackbuf[1] = ((d2 >> 6) + (d1 << 2)) & 0x1f;
      d1 = infiledata[dictptr++];
      unpackbuf[2] = (d2 >> 1) & 0x1f;
      unpackbuf[3] = ((d1 >> 4) + (d2 << 4)) & 0x1f;
      d2 = infiledata[dictptr++];
      unpackbuf[4] = ((d1 << 1) + (d2 >> 7)) & 0x1f;
      d1 = infiledata[dictptr++];
      unpackbuf[5] = (d2 >> 2) & 0x1f;
      unpackbuf[6] = ((d2 << 3) + (d1 >> 5)) & 0x1f;
      unpackbuf[7] = d1 & 0x1f;
      unpackcount = 1;
      return unpackbuf[0];
    }
}

/*
   decode word #wordnum (or all if wordnum==0xffff) from bank
   at #offset which starts with word #wordstart
 */

type16
do_dictbank (type16 offset, type16 wordstart, type16 wordnum)
{
/* decode 10 * 5 bytes */
  type16 currword = wordstart - 1;
  type8 i, lcount;
  type8 character, character2, wordletter[3];

/* start afresh with getdictionarycode() in each bank! */
  dictptr = offset;
  unpackcount = 8;

  while ((character = getdictionarycode ()) != 0x1b)
    {
      if (character > 0x1b)
	{
	  currword++;
	  lcount = 0;
	  if (wordnum == 0xffff)
	    {
	      fprintf (stdout, "\n%03x: ", currword);
	      new_mess = TRUE;
	    }
	  for (i = 0x1c; i < character; i++)
	    {
	      if ((wordnum == currword) || (wordnum == 0xffff))
		prt_char (wordletter[i - 0x1c]);
	      lcount++;
	    }
	}
      else if (character < 0x1a)
	{
	  if (lcount < 3)
	    wordletter[lcount++] = character + 0x61;
	  if ((wordnum == currword) || (wordnum == 0xffff))
	    prt_char (character + 0x61);
	}
      else
	/* character == 0x1a */
	{
/*
   0107 ['], 010d [-], 0110-0119 [0-9], 0201-021a [A-Z],
   0301-031a [a-z] (what for?)
 */
	  character = getdictionarycode ();
	  character2 = getdictionarycode ();

	  if (character < 0x08)
	    {
	      if (lcount < 3)
		wordletter[lcount++] = character * 0x20 + character2;
	      if ((wordnum == currword) || (wordnum == 0xffff))
		prt_char (character * 0x20 + character2);
	    }

/* should be taken out after testing all datafiles... */
	  if ((wordnum == currword) || (wordnum == 0xffff))
	    if (((character == 0x00) || (character > 0x03)) ||
		((character == 0x01) &&
		 ((character2 < 0x07) ||
		  ((character2 > 0x07) && (character2 < 0x0d)) ||
		  ((character2 > 0x0d) && (character2 < 0x10)) ||
		  (character2 > 0x19))) ||
		(((character == 0x02) || (character == 0x03)) &&
		 ((character2 == 0) || (character2 > 0x1a))))
	      fprintf (stdout, "***illegal char[%02x%02x]***",
		       character, character2);
	}
      if ((wordnum != 0xffff) && (currword > wordnum))
	break;
    }
  return (currword);
}

/*
   decode word #wordnum (or all if wordnum==0xffff)
 */

void
show_message_word (type16 wordref, type16 keywords);

void
do_messages (type16 keywords);

void
do_dictionary (type16 wordnum)
{
  type16 i, pcdict, wordstart;
  type16 prevpcdict = 0xffff, prevwordstart = 0xffff;
  type16 pctemp;

  if (wordnum == 0xffff)
    {
      fprintf (stdout, "\n");
      prtline ();
      fprintf (stdout, "Dictionary:\n");
      prtline ();
      fprintf (stdout, "Banks:\n");
    }

  pctemp = dictdata;
  for (i = 0; i <= dictdatalen; i++)
    {
      pcdict = defdict;
      wordstart = 0;
      if (i > 0)
	{
	  pcdict = calcwordLE (pctemp);
	  pctemp = pctemp + 2;
	  wordstart = calcwordLE (pctemp);
	  pctemp = pctemp + 2;
	}

      if (wordnum == 0xffff)
	{
	  fprintf (stdout, "\nBank %03x: ", i);
	  fprintf (stdout, "%04x (%03x) ", pcdict, wordstart);

	  if ((wordstart == prevwordstart) || (pcdict == prevpcdict))
	    fprintf (stdout, " [repeat]");
	  else
	    {
	      if ((type16) (highword + 1) > wordstart)
		fprintf (stdout, " *** DICT OVERLAP (%02x) ***",
			 highword + 1 - wordstart);
	      else if ((highword + 1) < wordstart)
		fprintf (stdout, "*** DICT GAP (%02x) ***",
			 wordstart - highword - 1);
	      fprintf (stdout, "\n");

	      highword = do_dictbank (pcdict, wordstart, 0xffff);

	    }
	  fprintf (stdout, "\n");
	}
      else if (wordnum < wordstart)
	break;

      prevwordstart = wordstart;
      prevpcdict = pcdict;
    }
  if (wordnum == 0xffff)
    {
      fprintf (stdout, "\nHighest word: #%04x\n\n", highword);
      fprintf (stdout, "Word table:\n");
      for (i = 0; i < 128; i++)
        {
	  type16 wordref = calcwordBE (wordtable + i * 2);
          fprintf (stdout, "%02x: ", i);
          new_mess = TRUE;
          show_message_word (wordref, FALSE);
          fprintf (stdout, "\n");
        }
      fprintf (stdout, "\nCommands:\n");
      do_messages(TRUE);
      fprintf (stdout, "\n");
    }
  else
    {
      prt_char (SEP /* '*'  */ );
      do_dictbank (prevpcdict, prevwordstart, wordnum);
    }
}

int IsDictionaryChar(char c)
{
  switch (c)
    {
      case '?': case '-': case '\'': case '/': return TRUE;
      case '!': case '.': case ',': return TRUE;
    }
    return isupper(c) || isdigit(c);
}

void
do_dictionary_v2 (type16 wordnum)
{
  type8 *ptr=infiledata+dictdata, x;
  type16 currword = 0;

  if (wordnum == 0xffff)
    {
      fprintf (stdout, "\n");
      prtline ();
      fprintf (stdout, "Dictionary:\n");
      prtline ();
    }

  do
    {
      currword++;
      if (wordnum == 0xffff)
        {
          fprintf (stdout, "\n%03x: ", currword);
	  new_mess = TRUE;
        }
      do
        {
          x = *ptr++;
          if (!IsDictionaryChar(x & 0x7f)) break;
          if (wordnum == 0xffff || currword == wordnum)
            if (x > 0) prt_char(x & 0x7f);
        }
      while (x > 0 && x < 0x7f);
      if (x == 0 || !IsDictionaryChar(x & 0x7f))
        {
          fprintf (stdout, "<BOGUS>");
          currword--;
          break;
	}
      ptr++;
      /* if (infiledata+enddictdata - ptr <= 1) break; */
      if (!IsDictionaryChar(*ptr & 0x7f)                           || \
          (*ptr <= 0x7f && !IsDictionaryChar(*(ptr+1) & 0x7f))     || \
          (*(ptr+1) <= 0x7f && !IsDictionaryChar(*(ptr+2) & 0x7f))    \
	 ) break;
    }
  while (currword < wordnum);
  if (wordnum == 0xffff)
    fprintf (stdout, "\n\nHighest word: #%04x\n\n", currword);
}

/*
   messages handling
 */

void
show_message_word (type16 wordref, type16 keywords)
{
  type16 d5, Off;

  d5 = (wordref >> 12) & 7;
  Off = wordref & 0x0fff;

  if (Off < 0x0f80)
    {
      if (Off <= highword)
        {
          if (!keywords || d5 == 1)
            {
              if (keywords)
                {
                  new_mess = TRUE;
	          fprintf (stdout, "%04x: ", keywords);
                }
	      do_dictionary (Off);
              if (keywords)
	        fprintf (stdout, "\n");
            }
        }
      else
	fprintf (stdout, "***highword!!![%04x]***", Off);
    }
  else if (Off == 0x0f80 && !keywords)
    {
      fprintf (stdout, "\n       [aka]");
      last_char = ',';		/* hack for no caps */
      count_in_line = 13;
    }
  else if (!keywords)
    {
      if (d5 & 2)
	prt_char (' ');
      Off &= 0x7f;
      prt_char ((type8) Off);
      if (d5 & 1)
	prt_char (' ');
    }
}


void
display_mess (type16 length, type16 keywords)
{
  type16 i = 0;
  type8 byte;
  type16 wordref;

  while (i < length)
    {
      byte = infiledata[pc++];

      if (byte >= 0x80)
	{
	  wordref = calcwordBE (pc - 1);
	  pc++;
	  i = i + 2;
#ifdef DICTDEBUG
	  fprintf (stdout, "[%04x]", wordref);
#endif
	}
      else
	{
/* one byte -> index into wordtable */
	  wordref = calcwordBE (wordtable + byte * 2);

#ifdef DICTDEBUG
	  fprintf (stdout, "{%02x->%04x}", byte, wordref);
#endif
	  i++;
	}
      show_message_word (wordref, keywords);
    }
  if (!keywords)
    fprintf (stdout, "\n");
}


void
do_messages (type16 keywords)
{
  type16 messnum = 0;
  type16 len, length;

  if (!keywords)
    {
      prtline ();
      fprintf (stdout, "Message database:\n");
      prtline ();
      fprintf (stdout, "\n");
    }

  pc = startmd;
  while (pc < endmd)
    {
#ifdef MESSNUMDEBUG
      fprintf (stdout, " [#%02x;L#%02x] ", messnum, infiledata[pc]);
#endif

      if (infiledata[pc] & 0x80)
	{
#ifdef MESSNUMDEBUG
	  fprintf (stdout, "#80L:%02x ", (infiledata[pc] - 0x80));
#endif
	  messnum = messnum + 1 + (infiledata[pc++] - 0x80);
	}
      else
	{
          if (keywords)
            keywords = messnum;
          else
	    fprintf (stdout, "M%04d: ", messnum);
	  new_mess = TRUE;
          length = 0;
	  do
	    length += len = ((signed)infiledata[pc++] - 1) & 0x3f;
	  while (len == 0x3f);
	  display_mess (length, keywords);
	  messnum++;
	}
    }
}

void printcharV2(char c)
{
        if (c==0x25) c=0xd;
        else if (c==0x5f) c=0x20;
        prt_char(c);
}

int msglenV2(type8 **ptr)
{
        int i=0;
        type8 a;

        /* catch berzerking code */
        if (*ptr >= infiledata+l9length) return 0;

        while ((a=**ptr)==0)
        {
         (*ptr)++;

         if (*ptr >= infiledata+l9length) return 0;

         i+=255;
        }
        i+=a;
        return i;
}

void displaywordV2(type8 *ptr,int msg)
{
        int n,messnum=msg;
	int mdV2=(ptr-infiledata==startmdV2-1);
        type8 a;
        if (msg==0) return;
        while (--msg)
        {
                ptr+=msglenV2(&ptr);
		if (!mdV2 && ptr-infiledata>=startmdV2-1) { end_mess = TRUE; return; }
        }
        n=msglenV2(&ptr);

	if (ptr<infiledata+startmdV2-1 && n>1 && *(ptr+1)>=3)
		fprintf (stdout, "\nM%04d: ", messnum);

        while (--n>0)
        {
                a=*++ptr;
                if (a<3) return;

                if (a>=0x5e) displaywordV2(infiledata+startmdV2-1,a-0x5d);
                else printcharV2((char)(a+0x1d));
        }
}

int msglenV1(type8 **ptr)
{
        type8 *ptr2=*ptr;
        while (ptr2<infiledata+l9length && *ptr2++>=3);
        return ptr2-*ptr;
}

void displaywordV1(type8 *ptr,int msg)
{
        int n,messnum=msg;
	int mdV2=(ptr-infiledata==startmdV2);
        type8 a;
        while (msg--)
        {
                ptr+=msglenV1(&ptr);
		if (!mdV2 && (ptr-infiledata>=startmdV2 || *(ptr-1) == 2)) { end_mess = TRUE; return; }
        }
        n=msglenV1(&ptr);

	if (ptr<infiledata+startmdV2 && n>1)
		fprintf (stdout, "\nM%04d: ", messnum);

        while (--n>0)
        {
                a=*ptr++;
                if (a<3) return;

                if (a>=0x5e) displaywordV1(infiledata+startmdV2,a-0x5e);
                else printcharV2((char)(a+0x1d));
        }
}

void printmessageV2(int Msg)
{
        if (infiledata[startmd]!=1) displaywordV2(infiledata+startmd,Msg);
        else displaywordV1(infiledata+startmd,Msg);
}

void
do_messages_v2 (void)
{
  type16 messnum = 0;

  prtline ();
  fprintf (stdout, "Message database:\n");
  prtline ();

  while (messnum++ < 9999 && !end_mess)
    {
       new_mess = TRUE;
       printmessageV2(messnum);
    }

  fprintf (stdout, "\n");
}

/*>>>>>>>>>>>>>>>>>>>>> main <<<<<<<<<<<<<<<<<<<<< */

int
main (int argc, char **argv)
{
  if (argc != 2)
    usage ();

/* open I/O streams */
#ifdef __TURBOC__
  _fmode = O_BINARY;
#endif

  if ((fdi = fopen (argv[1], "rb")) == NULL)
    ex ("infile not found");


/* get length of input file */
  fseek (fdi, 0L, SEEK_END);
  infilelength = ftell (fdi);
  fseek (fdi, 0L, SEEK_SET);

/* kludgy stuff for PCs, should be ifdef'd! */
  if (infilelength > 64000L)
    {
      infilelength = 64000L;
      fprintf (stderr, "Infile bigger than 64k; padding discarded...\n");
    }

/* next kludge */
  if (infilelength < 2L)
    ex ("file too short");

/* load complete input file into infiledata */
  if ((infiledata = malloc ((type16) (infilelength + 2))) == NULL)
    ex ("not enough memory");
  if (fread (infiledata, (type16) infilelength, 1, fdi) != 1)
    ex ("couldn't finish read");

/* 2 bytes of padding for exact length datafiles (PC) */
  infiledata[(type16) infilelength] = infiledata[(type16) infilelength + 1] = 0;
  infilelength = infilelength + 2;

/* calculate checksum */
  docheck ();

  fprintf (stdout, "Opening file \"%s\"...\n", argv[1]);
  if (L9V1Game != -1) fprintf (stdout, "V1 datafile found...\n");
  else if (isV2) fprintf (stdout, "V2 datafile found...\n");
  else fprintf (stdout, "V%d datafile found...\n", (l9length > V3LIMIT) ? 4 : 3);
  fprintf (stdout, "\nDatafile length: %04x\n", l9length);

/* read header */
  if (L9V1Game != -1) {
    /* acodeptr is calculated in ScanV1 */
    startmd = acodeptr + L9V1Games[L9V1Game].msgStart;
    startmdV2 = startmd + L9V1Games[L9V1Game].msgLen;
    endmd = startmdV2;
    endmdV2 = (startmdV2 < acodeptr) ? acodeptr : l9length;
    while (infiledata[endmdV2] == 0 || infiledata[endmdV2] == 0xff)
      endmdV2--;
    endmdV2++;
    absdatablock = acodeptr - L9V1Games[L9V1Game].absData;
    /* dictdata is calculated in ScanV1 */
    list1 = L9Pointers[2];
    list2 = L9Pointers[3];
    list3 = L9Pointers[4];
    list4 = L9Pointers[5];
    list5 = L9Pointers[6];
  } else if (isV2) {
    startmd = calcwordLE (0);
    startmdV2 = calcwordLE (2);
    endmd = startmdV2;
    absdatablock = calcwordLE (0x04);
    dictdata = calcwordLE (0x06);
    list1 = calcwordLE (0x08);
    list2 = calcwordLE (0x0a);
    list3 = calcwordLE (0x0c);
    list4 = calcwordLE (0x0e);
    list5 = calcwordLE (0x10);
    list6 = calcwordLE (0x12);
    list7 = calcwordLE (0x14);
    list8 = calcwordLE (0x16);
    list9 = calcwordLE (0x18);
    acodeptr = calcwordLE (0x1a);
    endmdV2 = acodeptr;
  } else {
    startmd = calcwordLE (2);
    endmd = startmd + calcwordLE (4);
    defdict = calcwordLE (6);
    enddict = defdict /* + 5 */  + calcwordLE (8);
    dictdata = calcwordLE (0x0a);
    dictdatalen = calcwordLE (0x0c);
    wordtable = calcwordLE (0x0e);
    unknown10 = calcwordLE (0x10);
    absdatablock = calcwordLE (0x12);
    list0 = calcwordLE (0x14);
    list1 = calcwordLE (0x16);
    list2 = calcwordLE (0x18);
    list3 = calcwordLE (0x1a);
    list4 = calcwordLE (0x1c);
    list5 = calcwordLE (0x1e);
    list6 = calcwordLE (0x20);
    list7 = calcwordLE (0x22);
    list8 = calcwordLE (0x24);
    list9 = calcwordLE (0x26);
    acodeptr = calcwordLE (0x28);
  }

  acodeend = (startmd > acodeptr) ? (startmd - 1) : (l9length - 1);
  while (infiledata[acodeend] == 0 || infiledata[acodeend] == 0xff)
    acodeend--;
  acodeend++;

/* print header */
  fprintf (stdout, "startmd:     %04x\n", startmd);
  fprintf (stdout, "endmd:       %04x\n", endmd);
  if (isV2) {
    fprintf (stdout, "startmdV2:   %04x\n", startmdV2);
    fprintf (stdout, "endmdV2:     %04x\n", endmdV2);
    fprintf (stdout, "absdatablock:%04x\n", absdatablock);
  }
  if (!isV2) {
    fprintf (stdout, "defdict:     %04x\n", defdict);
    fprintf (stdout, "enddict:     %04x\n", enddict);
  }
  fprintf (stdout, "dictdata:    %04x\n", dictdata);
  if (!isV2) {
    fprintf (stdout, "dictdatalen: %04x\n", dictdatalen);
    fprintf (stdout, "wordtable:   %04x\n", wordtable);
    fprintf (stdout, "unknown10:   %04x\n", unknown10);
    fprintf (stdout, "absdatablock:%04x\n", absdatablock);
    fprintf (stdout, "list0:       %04x\n", list0);
  }
  fprintf (stdout, "list1:       %04x\n", list1);
  fprintf (stdout, "list2:       %04x\n", list2);
  fprintf (stdout, "list3:       %04x\n", list3);
  fprintf (stdout, "list4:       %04x\n", list4);
  fprintf (stdout, "list5:       %04x\n", list5);
  if (L9V1Game == -1) {
    fprintf (stdout, "list6:       %04x\n", list6);
    fprintf (stdout, "list7:       %04x\n", list7);
    fprintf (stdout, "list8:       %04x\n", list8);
    fprintf (stdout, "list9:       %04x\n", list9);
  }
  fprintf (stdout, "acodeptr:    %04x\n", acodeptr);
  fprintf (stdout, "acodeend:    %04x\n", acodeend);

/*   do dictionary, get highword */
  if (isV2) do_dictionary_v2 (0xffff); else do_dictionary (0xffff); 

/* do messages, get highmess */
  if (isV2) do_messages_v2 (); else do_messages (FALSE);

/* do acode, get highcodemess */
/* try to print messages ! */
  for (pass = 1; pass <= 4; pass++)
    do_disassembly ();

/* cleanup */
  free (infiledata);
  fclose (fdi);
  return (0);
}
