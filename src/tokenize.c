/*
** 2001 September 15
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
*************************************************************************
** An tokenizer for SQL
**
** This file contains C code that splits an SQL input string up into
** individual tokens and sends those tokens one-by-one over to the
** parser for analysis.
*/
#include "sqliteInt.h"
#include <stdlib.h>

/*
** The charMap() macro maps alphabetic characters into their
** lower-case ASCII equivalent.  On ASCII machines, this is just
** an upper-to-lower case map.  On EBCDIC machines we also need
** to adjust the encoding.  Only alphabetic characters and underscores
** need to be translated.
*/
#ifdef SQLITE_ASCII
# define charMap(X) sqlite3UpperToLower[(unsigned char)X]
#endif
#ifdef SQLITE_EBCDIC
# define charMap(X) ebcdicToAscii[(unsigned char)X]
const unsigned char ebcdicToAscii[] = {
/* 0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F */
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  /* 0x */
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  /* 1x */
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  /* 2x */
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  /* 3x */
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  /* 4x */
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  /* 5x */
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 95,  0,  0,  /* 6x */
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  /* 7x */
   0, 97, 98, 99,100,101,102,103,104,105,  0,  0,  0,  0,  0,  0,  /* 8x */
   0,106,107,108,109,110,111,112,113,114,  0,  0,  0,  0,  0,  0,  /* 9x */
   0,  0,115,116,117,118,119,120,121,122,  0,  0,  0,  0,  0,  0,  /* Ax */
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  /* Bx */
   0, 97, 98, 99,100,101,102,103,104,105,  0,  0,  0,  0,  0,  0,  /* Cx */
   0,106,107,108,109,110,111,112,113,114,  0,  0,  0,  0,  0,  0,  /* Dx */
   0,  0,115,116,117,118,119,120,121,122,  0,  0,  0,  0,  0,  0,  /* Ex */
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  /* Fx */
};
#endif

/*
** The sqlite3KeywordCode function looks up an identifier to determine if
** it is a keyword.  If it is a keyword, the token code of that keyword is 
** returned.  If the input is not a keyword, TK_ID is returned.
**
** The implementation of this routine was generated by a program,
** mkkeywordhash.h, located in the tool subdirectory of the distribution.
** The output of the mkkeywordhash.c program is written into a file
** named keywordhash.h and then included into this source file by
** the #include below.
*/
#include "keywordhash.h"


/*
** If X is a character that can be used in an identifier then
** IdChar(X) will be true.  Otherwise it is false.
**
** For ASCII, any character with the high-order bit set is
** allowed in an identifier.  For 7-bit characters, 
** sqlite3IsIdChar[X] must be 1.
**
** For EBCDIC, the rules are more complex but have the same
** end result.
**
** Ticket #1066.  the SQL standard does not allow '$' in the
** middle of identifiers.  But many SQL implementations do. 
** SQLite will allow '$' in identifiers for compatibility.
** But the feature is undocumented.
*/
#ifdef SQLITE_ASCII
#define IdChar(C)  ((sqlite3CtypeMap[(unsigned char)C]&0x46)!=0)
#endif
#ifdef SQLITE_EBCDIC
const char sqlite3IsEbcdicIdChar[] = {
/* x0 x1 x2 x3 x4 x5 x6 x7 x8 x9 xA xB xC xD xE xF */
    0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0,  /* 4x */
    0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 0, 0,  /* 5x */
    0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0, 0,  /* 6x */
    0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0,  /* 7x */
    0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 0,  /* 8x */
    0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 0, 1, 0,  /* 9x */
    1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0,  /* Ax */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* Bx */
    0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1,  /* Cx */
    0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1,  /* Dx */
    0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1,  /* Ex */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0,  /* Fx */
};
#define IdChar(C)  (((c=C)>=0x42 && sqlite3IsEbcdicIdChar[c-0x40]))
#endif
int sqlite3IsIdChar(u8 c){ return IdChar(c); }


/*
** Return the length of the token that begins at z[0]. 
** Store the token type in *tokenType before returning.
*/
int sqlite3GetToken(const unsigned char *z, int *tokenType){
  int i, c;
  switch( *z ){
    case ' ': case '\t': case '\n': case '\f': case '\r': {
      testcase( z[0]==' ' );
      testcase( z[0]=='\t' );
      testcase( z[0]=='\n' );
      testcase( z[0]=='\f' );
      testcase( z[0]=='\r' );
      for(i=1; sqlite3Isspace(z[i]); i++){}
      *tokenType = TK_SPACE;
      return i;
    }
    case '-': {
      if( z[1]=='-' ){
        for(i=2; (c=z[i])!=0 && c!='\n'; i++){}
        *tokenType = TK_SPACE;   /* IMP: R-22934-25134 */
        return i;
      }
      *tokenType = TK_MINUS;
      return 1;
    }
    case '(': {
      *tokenType = TK_LP;
      return 1;
    }
    case ')': {
      *tokenType = TK_RP;
      return 1;
    }
    case ';': {
      *tokenType = TK_SEMI;
      return 1;
    }
    case '+': {
      *tokenType = TK_PLUS;
      return 1;
    }
    case '*': {
      *tokenType = TK_STAR;
      return 1;
    }
    case '/': {
      if( z[1]!='*' || z[2]==0 ){
        *tokenType = TK_SLASH;
        return 1;
      }
      for(i=3, c=z[2]; (c!='*' || z[i]!='/') && (c=z[i])!=0; i++){}
      if( c ) i++;
      *tokenType = TK_SPACE;   /* IMP: R-22934-25134 */
      return i;
    }
    case '%': {
      *tokenType = TK_REM;
      return 1;
    }
    case '=': {
      *tokenType = TK_EQ;
      return 1 + (z[1]=='=');
    }
    case '<': {
      if( (c=z[1])=='=' ){
        *tokenType = TK_LE;
        return 2;
      }else if( c=='>' ){
        *tokenType = TK_NE;
        return 2;
      }else if( c=='<' ){
        *tokenType = TK_LSHIFT;
        return 2;
      }else{
        *tokenType = TK_LT;
        return 1;
      }
    }
    case '>': {
      if( (c=z[1])=='=' ){
        *tokenType = TK_GE;
        return 2;
      }else if( c=='>' ){
        *tokenType = TK_RSHIFT;
        return 2;
      }else{
        *tokenType = TK_GT;
        return 1;
      }
    }
    case '!': {
      if( z[1]!='=' ){
        *tokenType = TK_ILLEGAL;
        return 2;
      }else{
        *tokenType = TK_NE;
        return 2;
      }
    }
    case '|': {
      if( z[1]!='|' ){
        *tokenType = TK_BITOR;
        return 1;
      }else{
        *tokenType = TK_CONCAT;
        return 2;
      }
    }
    case ',': {
      *tokenType = TK_COMMA;
      return 1;
    }
    case '&': {
      *tokenType = TK_BITAND;
      return 1;
    }
    case '~': {
      *tokenType = TK_BITNOT;
      return 1;
    }
    case '`':
    case '\'':
    case '"': {
      int delim = z[0];
      testcase( delim=='`' );
      testcase( delim=='\'' );
      testcase( delim=='"' );
      for(i=1; (c=z[i])!=0; i++){
        if( c==delim ){
          if( z[i+1]==delim ){
            i++;
          }else{
            break;
          }
        }
      }
      if( c=='\'' ){
        *tokenType = TK_STRING;
        return i+1;
      }else if( c!=0 ){
        *tokenType = TK_ID;
        return i+1;
      }else{
        *tokenType = TK_ILLEGAL;
        return i;
      }
    }
    case '.': {
#ifndef SQLITE_OMIT_FLOATING_POINT
      if( !sqlite3Isdigit(z[1]) )
#endif
      {
        *tokenType = TK_DOT;
        return 1;
      }
      /* If the next character is a digit, this is a floating point
      ** number that begins with ".".  Fall thru into the next case */
    }
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9': {
      testcase( z[0]=='0' );  testcase( z[0]=='1' );  testcase( z[0]=='2' );
      testcase( z[0]=='3' );  testcase( z[0]=='4' );  testcase( z[0]=='5' );
      testcase( z[0]=='6' );  testcase( z[0]=='7' );  testcase( z[0]=='8' );
      testcase( z[0]=='9' );
      *tokenType = TK_INTEGER;
#ifndef SQLITE_OMIT_HEX_INTEGER
      if( z[0]=='0' && (z[1]=='x' || z[1]=='X') && sqlite3Isxdigit(z[2]) ){
        for(i=3; sqlite3Isxdigit(z[i]); i++){}
        return i;
      }
#endif
      for(i=0; sqlite3Isdigit(z[i]); i++){}
#ifndef SQLITE_OMIT_FLOATING_POINT
      if( z[i]=='.' ){
        i++;
        while( sqlite3Isdigit(z[i]) ){ i++; }
        *tokenType = TK_FLOAT;
      }
      if( (z[i]=='e' || z[i]=='E') &&
           ( sqlite3Isdigit(z[i+1]) 
            || ((z[i+1]=='+' || z[i+1]=='-') && sqlite3Isdigit(z[i+2]))
           )
      ){
        i += 2;
        while( sqlite3Isdigit(z[i]) ){ i++; }
        *tokenType = TK_FLOAT;
      }
#endif
      while( IdChar(z[i]) ){
        *tokenType = TK_ILLEGAL;
        i++;
      }
      return i;
    }
    case '[': {
      for(i=1, c=z[0]; c!=']' && (c=z[i])!=0; i++){}
      *tokenType = c==']' ? TK_ID : TK_ILLEGAL;
      return i;
    }
    case '?': {
      *tokenType = TK_VARIABLE;
      for(i=1; sqlite3Isdigit(z[i]); i++){}
      return i;
    }
#ifndef SQLITE_OMIT_TCL_VARIABLE
    case '$':
#endif
    case '@':  /* For compatibility with MS SQL Server */
    case '#':
    case ':': {
      int n = 0;
      testcase( z[0]=='$' );  testcase( z[0]=='@' );
      testcase( z[0]==':' );  testcase( z[0]=='#' );
      *tokenType = TK_VARIABLE;
      for(i=1; (c=z[i])!=0; i++){
        if( IdChar(c) ){
          n++;
#ifndef SQLITE_OMIT_TCL_VARIABLE
        }else if( c=='(' && n>0 ){
          do{
            i++;
          }while( (c=z[i])!=0 && !sqlite3Isspace(c) && c!=')' );
          if( c==')' ){
            i++;
          }else{
            *tokenType = TK_ILLEGAL;
          }
          break;
        }else if( c==':' && z[i+1]==':' ){
          i++;
#endif
        }else{
          break;
        }
      }
      if( n==0 ) *tokenType = TK_ILLEGAL;
      return i;
    }
#ifndef SQLITE_OMIT_BLOB_LITERAL
    case 'x': case 'X': {
      testcase( z[0]=='x' ); testcase( z[0]=='X' );
      if( z[1]=='\'' ){
        *tokenType = TK_BLOB;
        for(i=2; sqlite3Isxdigit(z[i]); i++){}
        if( z[i]!='\'' || i%2 ){
          *tokenType = TK_ILLEGAL;
          while( z[i] && z[i]!='\'' ){ i++; }
        }
        if( z[i] ) i++;
        return i;
      }
      /* Otherwise fall through to the next case */
    }
#endif
    default: {
      if( !IdChar(*z) ){
        break;
      }
      for(i=1; IdChar(z[i]); i++){}
      *tokenType = keywordCode((char*)z, i);
      return i;
    }
  }
  *tokenType = TK_ILLEGAL;
  return 1;
}

/*
** Run the parser on the given SQL string.  The parser structure is
** passed in.  An SQLITE_ status code is returned.  If an error occurs
** then an and attempt is made to write an error message into 
** memory obtained from sqlite3_malloc() and to make *pzErrMsg point to that
** error message.
*/
int sqlite3RunParser(Parse *pParse, const char *zSql, char **pzErrMsg){
  int nErr = 0;                   /* Number of errors encountered */
  int i;                          /* Loop counter */
  void *pEngine;                  /* The LEMON-generated LALR(1) parser */
  int tokenType;                  /* type of the next token */
  int lastTokenParsed = -1;       /* type of the previous token */
  u8 enableLookaside;             /* Saved value of db->lookaside.bEnabled */
  sqlite3 *db = pParse->db;       /* The database connection */
  int mxSqlLen;                   /* Max length of an SQL string */

  assert( zSql!=0 );
  mxSqlLen = db->aLimit[SQLITE_LIMIT_SQL_LENGTH];
  if( db->nVdbeActive==0 ){
    db->u1.isInterrupted = 0;
  }
  pParse->rc = SQLITE_OK;
  pParse->zTail = zSql;
  i = 0;
  assert( pzErrMsg!=0 );
  pEngine = sqlite3ParserAlloc(sqlite3Malloc);
  if( pEngine==0 ){
    db->mallocFailed = 1;
    return SQLITE_NOMEM;
  }
  assert( pParse->pNewTable==0 );
  assert( pParse->pNewTrigger==0 );
  assert( pParse->nVar==0 );
  assert( pParse->nzVar==0 );
  assert( pParse->azVar==0 );
  enableLookaside = db->lookaside.bEnabled;
  if( db->lookaside.pStart ) db->lookaside.bEnabled = 1;
  while( !db->mallocFailed && zSql[i]!=0 ){
    assert( i>=0 );
    pParse->sLastToken.z = &zSql[i];
    pParse->sLastToken.n = sqlite3GetToken((unsigned char*)&zSql[i],&tokenType);
    i += pParse->sLastToken.n;
    if( i>mxSqlLen ){
      pParse->rc = SQLITE_TOOBIG;
      break;
    }
    switch( tokenType ){
      case TK_SPACE: {
        if( db->u1.isInterrupted ){
          sqlite3ErrorMsg(pParse, "interrupt");
          pParse->rc = SQLITE_INTERRUPT;
          goto abort_parse;
        }
        break;
      }
      case TK_ILLEGAL: {
        sqlite3ErrorMsg(pParse, "unrecognized token: \"%T\"",
                        &pParse->sLastToken);
        goto abort_parse;
      }
      case TK_SEMI: {
        pParse->zTail = &zSql[i];
        /* Fall thru into the default case */
      }
      default: {
        sqlite3Parser(pEngine, tokenType, pParse->sLastToken, pParse);
        lastTokenParsed = tokenType;
        if( pParse->rc!=SQLITE_OK ){
          goto abort_parse;
        }
        break;
      }
    }
  }
abort_parse:
  assert( nErr==0 );
  if( zSql[i]==0 && pParse->rc==SQLITE_OK && db->mallocFailed==0 ){
    if( lastTokenParsed!=TK_SEMI ){
      sqlite3Parser(pEngine, TK_SEMI, pParse->sLastToken, pParse);
      pParse->zTail = &zSql[i];
    }
    if( pParse->rc==SQLITE_OK && db->mallocFailed==0 ){
      sqlite3Parser(pEngine, 0, pParse->sLastToken, pParse);
    }
  }
#ifdef YYTRACKMAXSTACKDEPTH
  sqlite3_mutex_enter(sqlite3MallocMutex());
  sqlite3StatusSet(SQLITE_STATUS_PARSER_STACK,
      sqlite3ParserStackPeak(pEngine)
  );
  sqlite3_mutex_leave(sqlite3MallocMutex());
#endif /* YYDEBUG */
  sqlite3ParserFree(pEngine, sqlite3_free);
  db->lookaside.bEnabled = enableLookaside;
  if( db->mallocFailed ){
    pParse->rc = SQLITE_NOMEM;
  }
  if( pParse->rc!=SQLITE_OK && pParse->rc!=SQLITE_DONE && pParse->zErrMsg==0 ){
    pParse->zErrMsg = sqlite3MPrintf(db, "%s", sqlite3ErrStr(pParse->rc));
  }
  assert( pzErrMsg!=0 );
  if( pParse->zErrMsg ){
    *pzErrMsg = pParse->zErrMsg;
    sqlite3_log(pParse->rc, "%s", *pzErrMsg);
    pParse->zErrMsg = 0;
    nErr++;
  }
  if( pParse->pVdbe && pParse->nErr>0 && pParse->nested==0 ){
    sqlite3VdbeDelete(pParse->pVdbe);
    pParse->pVdbe = 0;
  }
#ifndef SQLITE_OMIT_SHARED_CACHE
  if( pParse->nested==0 ){
    sqlite3DbFree(db, pParse->aTableLock);
    pParse->aTableLock = 0;
    pParse->nTableLock = 0;
  }
#endif
#ifndef SQLITE_OMIT_VIRTUALTABLE
  sqlite3_free(pParse->apVtabLock);
#endif

  if( !IN_DECLARE_VTAB ){
    /* If the pParse->declareVtab flag is set, do not delete any table 
    ** structure built up in pParse->pNewTable. The calling code (see vtab.c)
    ** will take responsibility for freeing the Table structure.
    */
    sqlite3DeleteTable(db, pParse->pNewTable);
  }

  if( pParse->bFreeWith ) sqlite3WithDelete(db, pParse->pWith);
  sqlite3DeleteTrigger(db, pParse->pNewTrigger);
  for(i=pParse->nzVar-1; i>=0; i--) sqlite3DbFree(db, pParse->azVar[i]);
  sqlite3DbFree(db, pParse->azVar);
  while( pParse->pAinc ){
    AutoincInfo *p = pParse->pAinc;
    pParse->pAinc = p->pNext;
    sqlite3DbFree(db, p);
  }
  while( pParse->pZombieTab ){
    Table *p = pParse->pZombieTab;
    pParse->pZombieTab = p->pNextZombie;
    sqlite3DeleteTable(db, p);
  }
  assert( nErr==0 || pParse->rc!=SQLITE_OK );
  return nErr;
}
