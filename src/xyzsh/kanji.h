/* 
 * 漢字関連ライブラリ
 */

#ifndef XYZSH_KANJI_H
#define XYZSH_KANJI_H

#include <wchar.h>

enum eKanjiCode { kEucjp, kSjis, kUtf8, kUtf8Mac, kByte, kUnknown };
extern char* gKanjiCodeString[6];
extern char* gIconvKanjiCodeName[6];     // ICONVの漢字コードの名前

void kanji_init();     // 初期化
void kanji_final();    // 解放

int str_pointer2kanjipos(enum eKanjiCode code, char* mbs, char* point);
    // mbsをeucjp,sjis,UTF文字列と見たときのpointの場所は何文字目かを返す
char* str_kanjipos2pointer(enum eKanjiCode code, char* mbs, int pos);
    // mbsをUTF文字列と見たときのpos文字目の位置を返す

BOOL is_utf8_bytes(char* mbs);
    // 文字列がUTf8文字列かどうか

extern int is_hankaku(enum eKanjiCode code, unsigned char c);
    // 引数が半角文字一バイト目かどうか
extern int is_kanji(enum eKanjiCode code, unsigned char c);
    // 引数が漢字一バイト目かどうか
int is_all_ascii(enum eKanjiCode code, char* buf);
    // FALSE:漢字まじり TRUE:全部英数字

extern int str_termlen(enum eKanjiCode code, char* mbs);
    // 引数の文字列が画面で何文字か
extern int str_termlen2(enum eKanjiCode code, char* mbs, int pos);
    // 漢字でpos文字目までの端末で何文字か
int str_termlen_range(enum eKanjiCode code, char* mbs, int utfpos1, int utfpos2);
    // mbsを漢字と見たときutfpo1文字目からutfpos2文字目までのは端末で何文字分か
extern int str_kanjilen(enum eKanjiCode code, char* mbs);
    // 引数の文字列が漢字で何文字か
    // 戻り値が-1ならSIGINTくらっている

#if defined(HAVE_ICONV_H) || defined(HAVE_BICONV_H)

enum eKanjiCode kanji_encode_type(char* buf);
    // 引数の文字列の漢字エンコードを返す

int kanji_convert(char* input_buf, char* output_buf, size_t output_buf_size, enum eKanjiCode input_kanji_encode_type, enum eKanjiCode output_kanji_encode_type);
    // 漢字エンコードの変換。iconvのラッパー
    // output_bufは十分なスペースが割り当てられている必要がある
    // 返り値は-1はエラー 0は成功

#endif

#endif

