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

#endif /* SAKURA_CONVERT_UTIL2_9F00219B_A2FC_4096_BB26_197A667DFD25_H_ */
