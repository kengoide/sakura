﻿/*! @file */
/*
	Copyright (C) 2018-2021, Sakura Editor Organization

	This software is provided 'as-is', without any express or implied
	warranty. In no event will the authors be held liable for any damages
	arising from the use of this software.

	Permission is granted to anyone to use this software for any purpose,
	including commercial applications, and to alter it and redistribute it
	freely, subject to the following restrictions:

		1. The origin of this software must not be misrepresented;
		   you must not claim that you wrote the original software.
		   If you use this software in a product, an acknowledgment
		   in the product documentation would be appreciated but is
		   not required.

		2. Altered source versions must be plainly marked as such,
		   and must not be misrepresented as being the original software.

		3. This notice may not be removed or altered from any source
		   distribution.
*/
#include "StdAfx.h"
#include "CDecode_UuDecode.h"
#include "charset/charcode.h"
#include "convert/convert_util2.h"
#include "util/string_ex2.h"
#include "CEol.h"
#include "mem/CNativeW.h"

/*
	Unix-to-Unix のこと

begin <permission> <file name>
<encoded data>

end

<permission>：
	ファイル生成時に使うパーミッションの値
	（Windowsではパーミッションが存在しない？ので、600 または 666 を用いる）

<file name>：
	ファイル生成時に使いファイル名

<encoded data>：
	・バイナリデータを3バイトずつ取り出し、それら3バイトをMSBからLSBへと並べた
	　24ビット幅のデータをさらに4分割し、MSBから順に6ビットずつ取り出す。
	　取り出したそれぞれの値に 0x20(空白文字)を加算し7ビットASCII文字に変換し、
	　取り出した順に書き込んでいく。
	・データ長が3の倍数になっていない場合は、0でパディングして3の倍数となるよう調節する。
	・行の最初には、その行に何バイト分のデータがあるかの情報を書き込む。
	・1行には45バイト分のデータ（60文字）を書き込むのが慣例で（決まり？）、最後の行以外は、
	　"M"（45+0x20）が行の先頭となる。
	・符号化されたデータは、0バイトの行で終了する。
	・行末の空白を削除するゲートウェイに対処するため、空白は、"~"(0x7E)または"`"(0x60)を換わりに使う。
*/

inline BYTE _UUDECODE_CHAR(WCHAR c) {
	BYTE c_ = (c & 0xff);
	if (c_ == L'`' || c_ == L'~') {
		c_ = L' ';
	}
	return static_cast<BYTE>((static_cast<BYTE>(c_) - 0x20) & 0x3f);
}
inline BYTE _UUDECODE_CHAR(ACHAR c) {
	if (c == '`' || c == '~') {
		c = ' ';
	}
	return static_cast<BYTE>((static_cast<BYTE>(c) - 0x20) & 0x3f);
}

/*
	UU デコーダー（一行だけ実行するバージョン）

	@param[in] nSrcLen	必ず、4の倍数であること。
	@param[in] pDest	必ず、(nSrcLen / 4) * 3 以上のバッファが確保されていること。

	@return 一行分をデコードした結果得られた生データのバイト長
			書き込んだデータが戻り値よりも大きいときがあるので注意。
*/
template< class CHAR_TYPE >
int _DecodeUU_line(const CHAR_TYPE* pSrc, const int nSrcLen, char* pDest) {
	unsigned long lDataDes;
	const CHAR_TYPE* pr;

	if (nSrcLen < 1) {
		return 0;
	}

	pr = pSrc + 1;  // 先頭の文字（M(0x20+45)など）を飛ばす
	int i = 0;
	int j = 0;
	int k = 0;
	for (; i < nSrcLen; i += 4) {
		lDataDes = 0;
		for (j = 0; j < 4; ++j) {
			lDataDes |= _UUDECODE_CHAR(pr[i + j]) << ((4 - j - 1) * 6);
		}
		for (j = 0; j < 3; ++j) {
			pDest[k + j] = (char)((lDataDes >> ((3 - j - 1) * 8)) & 0x000000ff);
		}
		k += 3;
	}

	return _UUDECODE_CHAR(pSrc[0]); // 1行分をデコードしたときに得られる生データのバイト長を取得
}

/*!
	UUエンコードのヘッダー部分を解析
*/
template< class CHAR_TYPE >
bool CheckUUHeader(const CHAR_TYPE* pSrc, const int nLen, WCHAR* pszFilename) {
	//	using namespace WCODE;

	const CHAR_TYPE* pr, * pr_end;
	CHAR_TYPE* pwstart;
	int nwlen, nstartidx;
	CHAR_TYPE pszSplitChars[16];

	if (sizeof(CHAR_TYPE) == 2) {
		// スペースまたはタブが区切り文字
		pszSplitChars[0] = L' ';
		pszSplitChars[1] = L'\t';
		pszSplitChars[2] = L'\0';
	}
	else {
		// スペースまたはタブが区切り文字
		pszSplitChars[0] = ' ';
		pszSplitChars[1] = '\t';
		pszSplitChars[2] = '\0';
	}

	if (nLen < 1) {
		if (pszFilename) {
			pszFilename[0] = L'\0';
		}
		return false;
	}

	// 先頭の空白・改行文字をスキップ
	for (nstartidx = 0; nstartidx < nLen; ++nstartidx) {
		CHAR_TYPE c = pSrc[nstartidx];
		if (sizeof(CHAR_TYPE) == 2) {
			if (c != L'\r' && c != L'\n' && c != L' ' && c != L'\t') {
				break;
			}
		}
		else {
			if (c != '\r' && c != '\n' && c != ' ' && c != '\t') {
				break;
			}
		}
	}

	pr = pSrc + nstartidx;
	pr_end = pSrc + nLen;

	// ヘッダーの構成
	// begin  755  <filename>

	/* begin を取得 */

	pr += CWordParse::GetWord(pr, pr_end - pr, pszSplitChars, &pwstart, &nwlen);
	if (nwlen != 5) {
		// error.
		return false;
	}
	if (sizeof(CHAR_TYPE) == 2) {
		if (wcsncmp_literal(pwstart, L"begin") != 0) {
			// error.
			return false;
		}
	}
	else {
		if (strncmp_literal(reinterpret_cast<const char*>(pwstart), "begin") != 0) {
			// error.
			return false;
		}
	}

	/* 3桁の8進数（Unix システムのパーミッション）を取得 */

	pr += CWordParse::GetWord(pr, pr_end - pr, pszSplitChars, &pwstart, &nwlen);
	if (nwlen != 3) {
		// error.
		return false;
	}
	for (int i = 0; i < nwlen; i++) {
		if (sizeof(CHAR_TYPE) == 2) {
			// WCHAR の場合の処理
			if (!iswdigit(pwstart[i]) || (pwstart[i] == L'8' || pwstart[i] == L'9')) {
				// error.
				return false;
			}
		}
		else {
			// ACHAR の場合の処理
			if (!isdigit(pwstart[i]) || (pwstart[i] == '8' || pwstart[i] == '9')) {
				// error.
				return false;
			}
		}
	}

	/* 書き出し用のファイル名を取得 */

	pr += CWordParse::GetWord(pr, pr_end - pr, pszSplitChars, &pwstart, &nwlen);
	// 末尾の空白・改行文字をスキップ
	for (; nwlen > 0; --nwlen) {
		CHAR_TYPE c = pwstart[nwlen - 1];
		if (sizeof(CHAR_TYPE) == 2) {
			if (!WCODE::IsLineDelimiterBasic(c) && c != L' ' && c != L'\t') {
				break;
			}
		}
		else {
			if (c != '\r' && c != '\n' && c != ' ' && c != '\t') {
				break;
			}
		}
	}
	if (nwlen < 1 || nwlen + 1  > _MAX_PATH) {
		// error.
		return false;
	}
	// ファイル名を格納
	if (pszFilename) {
		strtotcs(pszFilename, pwstart, (size_t)nwlen);
		pszFilename[nwlen] = L'\0';
	}

	return true;
}

/*!
	UU フッターを確認
*/
template< class CHAR_TYPE >
bool CheckUUFooter(const CHAR_TYPE* pS, const int nLen) {
	int nstartidx;
	const CHAR_TYPE* psrc;
	int nsrclen;
	int i;

	// フッターの構成
	// end
	// ※ 空行はフッターに含めない。

	// 先頭の改行・空白文字をスキップ
	for (nstartidx = 0; nstartidx < nLen; ++nstartidx) {
		CHAR_TYPE c = pS[nstartidx];
		if (sizeof(CHAR_TYPE) == 2) {
			// WCHAR の場合の処理
			if (c != L'\r' && c != L'\n' && c != L' ' && c != L'\t') {
				break;
			}
		}
		else {
			// ACHAR の場合の処理
			if (c != '\r' && c != '\n' && c != ' ' && c != '\t') {
				break;
			}
		}
	}

	psrc = pS + nstartidx;
	nsrclen = nLen - nstartidx;
	i = 0;

	if (nsrclen < 3) {
		return false;
	}
	if (sizeof(CHAR_TYPE) == 2) {
		if (wcsncmp_literal(&pS[nstartidx], L"end") != 0) {
			// error.
			return false;
		}
	}
	else {
		if (strncmp_literal(reinterpret_cast<const char*>(&pS[nstartidx]), "end") != 0) {
			// error.
			return false;
		}
	}
	i += 3;

	// end の後が空白文字ばかりであることを確認
	for (; i < nsrclen; ++i) {
		CHAR_TYPE c = psrc[i];
		if (sizeof(CHAR_TYPE) == 2) {
			// WCHAR の場合の処理
			if (!WCODE::IsLineDelimiterBasic(c) && c != L' ' && c != L'\t') {
				return false;
			}
		}
		else {
			// ACHAR の場合の処理
			if (c != '\r' && c != '\n' && c != ' ' && c != '\t') {
				return false;
			}
		}
	}

	return true;
}

/* Uudecode (デコード）*/
bool CDecode_UuDecode::DoDecode( const CNativeW& pcSrc, CMemory* pcDst )
{
	const WCHAR *psrc, *pline;
	int nsrclen;
	char *pw, *pw_base;
	int nlinelen, ncuridx;
	CEol ceol;
	bool bsuccess = false;

	pcDst->Reset();
	psrc = pcSrc.GetStringPtr();
	nsrclen = pcSrc.GetStringLength();

	if( nsrclen < 1 ){
		pcDst->_AppendSz("");
		return false;
	}
	pcDst->AllocBuffer( (nsrclen / 4) * 3 + 10 );
	pw_base = pw = reinterpret_cast<char *>( pcDst->GetRawPtr() );

	// 先頭の改行・空白文字をスキップ
	for( ncuridx = 0; ncuridx < nsrclen; ++ncuridx ){
		WCHAR c = psrc[ncuridx];
		if( !WCODE::IsLineDelimiterBasic(c) && c != L' ' && c != L'\t' ){
			break;
		}
	}

	// ヘッダーを解析
	pline = GetNextLineW( psrc, nsrclen, &nlinelen, &ncuridx, &ceol, false );
	if( !CheckUUHeader(pline, nlinelen, m_aFilename) ){
		pcDst->_AppendSz("");
		return false;
	}

	// ボディーを処理
	while( (pline = GetNextLineW(psrc, nsrclen, &nlinelen, &ncuridx, &ceol, false)) != NULL ){
		if( ceol != EEolType::cr_and_lf ){
			pcDst->_AppendSz("");
			return false;
		}
		if( nlinelen < 1 ){
			pcDst->_AppendSz("");
			return false;
		}
		if( nlinelen == 1 ){
			// データの最後である場合
			if( pline[0] == L' ' || pline[0] == L'`' || pline[0] == L'~' ){
				bsuccess = true;
				break;
			}
		}
		pw += _DecodeUU_line( pline, nlinelen, pw );
	}
	if( bsuccess == false ){
		return false;
	}

	pline += 3;  // '`' 'CR' 'LF' の分をスキップ

	// フッターを解析
	if( !CheckUUFooter(pline, nsrclen-ncuridx) ){
		pcDst->_AppendSz("");
		return false;
	}

	pcDst->_SetRawLength( pw - pw_base );
	return true;
}
