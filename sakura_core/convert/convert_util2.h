/*!	@file
	@brief 変換ユーティリティ2 - BASE64 Ecode/Decode, UUDecode, Q-printable decode

	@author 
*/

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
#ifndef SAKURA_CONVERT_UTIL2_9F00219B_A2FC_4096_BB26_197A667DFD25_H_
#define SAKURA_CONVERT_UTIL2_9F00219B_A2FC_4096_BB26_197A667DFD25_H_
#pragma once

#include "charset/charcode.h"
#include "parse/CWordParse.h"
#include "mem/CMemory.h"
#include "util/string_ex.h"
#include "charset/charset.h"

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
//
//    Quoted-Printable デコード
//
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
//

inline ACHAR _GetHexChar( ACHAR c )
{
	if( (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') ){
		return c;
	}else if( c >= 'a' && c <= 'f' ){
		return  c - ('a' - 'A');
	}else{
		return '\0';
	}
}
inline WCHAR _GetHexChar( WCHAR c )
{
	if( (c >= L'0' && c <= L'9') || (c >= L'A' && c <= L'F') ){
		return c;
	}else if( c >= L'a' && c <= L'f' ){
		return  c - (L'a' - L'A');
	}else{
		return L'\0';
	}
}

/*
	c の入力値： 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, A, B, C, D, E, F
*/
inline int _HexToInt( ACHAR c )
{
	if( c <= '9' ){
		return c - '0';
	}else{
		return c - 'A' + 10;
	}
}
inline int _HexToInt( WCHAR c )
{
	if( c <= L'9' ){
		return c - L'0';
	}else{
		return c - L'A' + 10;
	}
}

template< class CHAR_TYPE >
int _DecodeQP( const CHAR_TYPE* pS, const int nLen, char* pDst )
{
	const CHAR_TYPE *pr;
	char *pw;
	int ninc_len;

	pr = pS;
	pw = pDst;

	while( pr < pS + nLen ){
		/* =XX の形式でない部分をデコード */
		if( sizeof(CHAR_TYPE) == 2 ){
			if( *pr != L'=' ){
				*pw = static_cast<char>( *pr );
				pw += 1;
				pr += 1;
				continue;
			}
		}else{
			if( *pr != '=' ){
				*pw = static_cast<char>( *pr );
				pw += 1;
				pr += 1;
				continue;
			}
		}

		/* =XX の部分をデコード */
		ninc_len = 1;   // '=' の部分のインクリメント。
		if( pr + 2 < pS + nLen ){
			// デコード実行部分
			CHAR_TYPE c1, c2;
			c1 = _GetHexChar(pr[1]);
			c2 = _GetHexChar(pr[2]);
			if( c1 != 0 && c2 != 0 ){
				*pw = static_cast<char>(_HexToInt(c1) << 4) | static_cast<char>(_HexToInt(c2));
				++pw;
			}else{
				pw[0] = '=';
				pw[1] = static_cast<char>(pr[1] & 0x00ff);
				pw[2] = static_cast<char>(pr[2] & 0x00ff);
				pw += 3;
			}
			ninc_len += 2;
			// ここまで。
		}
		pr += ninc_len;
	}

	return pw - pDst;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
//
// BAASE64 のエンコード/デコード
//
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
//

extern const uchar_t TABLE_BASE64CharToValue[];
extern const char TABLE_ValueToBASE64Char[];

// BASE64文字 <-> 数値
template< class CHAR_TYPE >
inline uchar_t Base64ToVal( const CHAR_TYPE c ){
	int c_ = c;
	return static_cast<uchar_t>((c_ < 0x80)? TABLE_BASE64CharToValue[c_] : -1);
}
template< class CHAR_TYPE >
inline CHAR_TYPE ValToBase64( const char v ){
	int v_ = v;
	return static_cast<CHAR_TYPE>((v_ < 64)? TABLE_ValueToBASE64Char[v_] : -1);
}

#if 0
/*
	Bas64文字列の末尾が適切かどうかをチェック
	
	入力：BASE64 文字列。
*/
template< class CHAR_TYPE >
bool CheckBase64Padbit( const CHAR_TYPE *pSrc, const int nSrcLen )
{
	bool bret = true;

	if( nSrcLen < 1 ){
		return false;
	}

	/* BASE64文字の末尾について：
		ooxx xxxx   ooxx oooo                   -> 1 byte(s)
		ooxx xxxx   ooxx xxxx   ooxx xxoo          -> 2 byte(s)
		ooxx xxxx   ooxx xxxx   ooxx xxxx   ooxx xxxx -> 3 byte(s)
	*/
	
	switch( nSrcLen % 4 ){
	case 0:
		break;
	case 1:
		bret = false;
		break;
	case 2:
		if( (Base64ToVal(pSrc[nSrcLen-1]) & 0x0f) != 0 ){
			bret = false;
		}
		break;
	case 3:
		if( (Base64ToVal(pSrc[nSrcLen-1]) & 0x03) != 0 ){
			bret = false;
		}
		break;
	}
	return bret;
}
#endif

/*!
	BASE64 デコード実行関数

	前の実装を参考に。
	正しい BASE64 入力文字列を仮定している。
*/
template< class CHAR_TYPE >
int _DecodeBase64( const CHAR_TYPE *pSrc, const int nSrcLen, char *pDest )
{
	long lData;
	int nDesLen;
	int sMax;
	int nsrclen = nSrcLen;

	// 文字列の最後のパッド文字 '=' を文字列長に含めないようにする処理
	{
		int i = 0;
		bool bret;
		for( ; i < nsrclen; i++ ){
			if( sizeof(CHAR_TYPE) == 2 ){
				bret = ( pSrc[nsrclen-1-i] == L'=' );
			}else{
				bret = ( pSrc[nsrclen-1-i] == '=' );
			}
			if( bret != true ){
				break;
			}
		}
		nsrclen -= i;
	}

	nDesLen = 0;
	for( int i = 0; i < nsrclen; i++ ){
		if( i < nsrclen - (nsrclen % 4) ){
			sMax = 4;
		}else{
			sMax = (nsrclen % 4);
		}
		lData = 0;
		for( int j = 0; j < sMax; j++ ){
			long k = Base64ToVal( pSrc[i + j] );
			lData |= k << ((4 - j - 1) * 6);
		}
		for( int j = 0; j < (sMax * 6)/ 8 ; j++ ){
			pDest[nDesLen] = static_cast<char>((lData >> (8 * (2 - j))) & 0x0000ff);
			nDesLen++;
		}
		i+= 3;
	}
	return nDesLen;
}

/*!
	BASE64 エンコード実行関数

	前の実装を参考に。
	パッド文字などは付加しない。エラーチェックなし。
*/
template< class CHAR_TYPE >
int _EncodeBase64( const char *pSrc, const int nSrcLen, CHAR_TYPE *pDest )
{
	const unsigned char *psrc;
	unsigned long lDataSrc;
	int i, j, k, n;
	char v;
	int nDesLen;

	psrc = reinterpret_cast<const unsigned char *>(pSrc);
	nDesLen = 0;
	for( i = 0; i < nSrcLen; i += 3 ){
		lDataSrc = 0;
		if( nSrcLen - i < 3 ){
			n = nSrcLen % 3;
			j = (n * 4 + 2) / 3;  // 端数切り上げ
		}else{
			n = 3;
			j = 4;
		}
		// n 今回エンコードする長さ
		// j エンコード後のBASE64文字数
		for( k = 0; k < n; k++ ){
			lDataSrc |=
				static_cast<unsigned long>(psrc[i + k]) << ((n - k - 1) * 8);
		}
		// パッドビット付加。lDataSrc の長さが 6*j になるように調節する。
		lDataSrc <<= j * 6 - n * 8;
		// エンコードして書き込む。
		for( k = 0; k < j; k++ ){
			v = static_cast<char>((lDataSrc >> (6 * (j - k - 1))) & 0x0000003f);
			pDest[nDesLen] = static_cast<CHAR_TYPE>(ValToBase64<CHAR_TYPE>( v ));
			nDesLen++;
		}
	}
	return nDesLen;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
//
//    MIME ヘッダーデコード
//
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
//

enum EEncodingMethod {
	EM_NONE,
	EM_QP,
	EM_BASE64,
};

/*!
	MIMEヘッダーデコード補助関数

	@return  CMemory と置き換えられる入力文字列長 (nSkipLen)
*/
template< class CHAR_TYPE >
int _DecodeMimeHeader( const CHAR_TYPE* pSrc, const int nSrcLen, CMemory* pcMem_alt, ECodeType* peCodetype )
{
	ECodeType ecode = CODE_NONE;
	EEncodingMethod emethod = EM_NONE;
	int nLen_part1, nLen_part2, nskipped_len;
	int ncmpresult1, ncmpresult2, ncmpresult;

	const CHAR_TYPE *pr, *pr_base;
	char* pdst;
	int ndecoded_len;

	// MIME の該当部分を検出。----------------------------------------
	//

	//   part1 部分
	//
	//   "=?ISO-2022-JP?", "=?UTF-8?" などの部分を検出
	//

	if( pSrc+14 < pSrc+nSrcLen ){
		// JIS の場合
		if( sizeof(CHAR_TYPE) == 2 ){
			ncmpresult = wcsnicmp_literal( reinterpret_cast<const wchar_t*>(&pSrc[0]), L"=?ISO-2022-JP?" );
		}else{
			ncmpresult = strnicmp_literal( &pSrc[0], "=?ISO-2022-JP?" );
		}
		if( ncmpresult == 0 ){  // 
			ecode = CODE_JIS;
			nLen_part1 = 14;
			goto finish_first_detect;
		}
	}
	if( pSrc+8 < pSrc+nSrcLen ){
		// UTF-8 の場合
		if( sizeof(CHAR_TYPE) == 2 ){
			ncmpresult = wcsnicmp_literal( reinterpret_cast<const wchar_t*>(&pSrc[0]), L"=?UTF-8?" );
		}else{
			ncmpresult = strnicmp_literal( &pSrc[0], "=?UTF-8?" );
		}
		if( ncmpresult == 0 ){
			ecode = CODE_UTF8;
			nLen_part1 = 8;
			goto finish_first_detect;
		}
	}
	// マッチしなかった場合
	pcMem_alt->SetRawData( "", 0 );
	if( peCodetype ){
		*peCodetype = CODE_NONE;
	}
	return 0;

finish_first_detect:;

	if( peCodetype ){
		*peCodetype = ecode;
	}

	//
	//    part2 部分
	//
	//   "B?" または "Q?" の部分を検出
	//

	if( pSrc+nLen_part1+2 >= pSrc+nSrcLen ){
		pcMem_alt->SetRawData( "", 0 );
		return 0;
	}
	if( sizeof(CHAR_TYPE) == 2 ){
		ncmpresult1 = wcsnicmp_literal( reinterpret_cast<const wchar_t*>(&pSrc[nLen_part1]), L"B?" );
		ncmpresult2 = wcsnicmp_literal( reinterpret_cast<const wchar_t*>(&pSrc[nLen_part1]), L"Q?" );
	}else{
		ncmpresult1 = strnicmp_literal( &pSrc[nLen_part1], "B?" );
		ncmpresult2 = strnicmp_literal( &pSrc[nLen_part1], "Q?" );
	}
	if( ncmpresult1 == 0 ){
		emethod = EM_BASE64;
	}else if( ncmpresult2 == 0 ){
		emethod = EM_QP;
	}else{
		pcMem_alt->SetRawData( "", 0 );
		return 0;
	}
	nLen_part2 = 2;

	//
	//   エンコード文字列の部分を検出
	//

	pr_base = pSrc + nLen_part1 + nLen_part2;
	pr = pSrc + nLen_part1 + nLen_part2;
	for( ; pr < pSrc+nSrcLen-1; ++pr ){
		if( sizeof(CHAR_TYPE) == 2 ){
			ncmpresult = wcsncmp_literal( reinterpret_cast<const wchar_t*>(pr), L"?=" );
		}else{
			ncmpresult = strncmp_literal( pr, "?=" );
		}
		if( ncmpresult == 0 ){
			break;
		}
	}
	if( pr == pSrc+nSrcLen-1 ){
		pcMem_alt->SetRawData( "", 0 );
		return 0;
	}

	nskipped_len = pr - pSrc + 2;  // =? から ?= までの、全体の長さを記録

	//   デコード ----------------------------------------------------
	//

	pcMem_alt->AllocBuffer( pr - pr_base );
	pdst = reinterpret_cast<char*>( pcMem_alt->GetRawPtr() );
	if( pdst == NULL ){
		pcMem_alt->SetRawData( "", 0 );
		return 0;
	}

	if( emethod == EM_BASE64 ){
		ndecoded_len = _DecodeBase64( pr_base, pr-pr_base, pdst );
	}else{
		ndecoded_len = _DecodeQP( pr_base, pr-pr_base, pdst );
	}

	pcMem_alt->_SetRawLength( ndecoded_len );

	return nskipped_len;
}
#endif /* SAKURA_CONVERT_UTIL2_9F00219B_A2FC_4096_BB26_197A667DFD25_H_ */
