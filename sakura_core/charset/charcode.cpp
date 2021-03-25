/*! @file */
/*
	Copyright (C) 2007, kobake
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
#include "charset/charcode.h"
#include <algorithm>
#include <array>

/*! キーワードキャラクタ */
const std::array<unsigned char, 128> gm_keyword_char = {
 /* 0         1         2         3         4         5         6         7         8         9         A         B         C         D         E         F             : 0123456789ABCDEF */
	CK_CTRL,  CK_CTRL,  CK_CTRL,  CK_CTRL,  CK_CTRL,  CK_CTRL,  CK_CTRL,  CK_CTRL,  CK_CTRL,  CK_TAB,   CK_LF,    CK_CTRL,  CK_CTRL,  CK_CR,    CK_CTRL,  CK_CTRL,  /* 0: ................ */
	CK_CTRL,  CK_CTRL,  CK_CTRL,  CK_CTRL,  CK_CTRL,  CK_CTRL,  CK_CTRL,  CK_CTRL,  CK_CTRL,  CK_CTRL,  CK_CTRL,  CK_CTRL,  CK_CTRL,  CK_CTRL,  CK_CTRL,  CK_CTRL,  /* 1: ................ */
	CK_SPACE, CK_ETC,   CK_ETC,   CK_UDEF,  CK_UDEF,  CK_ETC,   CK_ETC,   CK_ETC,   CK_ETC,   CK_ETC,   CK_ETC,   CK_ETC,   CK_ETC,   CK_ETC,   CK_ETC,   CK_ETC,   /* 2:  !"#$%&'()*+,-./ */
	CK_CSYM,  CK_CSYM,  CK_CSYM,  CK_CSYM,  CK_CSYM,  CK_CSYM,  CK_CSYM,  CK_CSYM,  CK_CSYM,  CK_CSYM,  CK_ETC,   CK_ETC,   CK_ETC,   CK_ETC,   CK_ETC,   CK_ETC,   /* 3: 0123456789:;<=>? */
	CK_UDEF,  CK_CSYM,  CK_CSYM,  CK_CSYM,  CK_CSYM,  CK_CSYM,  CK_CSYM,  CK_CSYM,  CK_CSYM,  CK_CSYM,  CK_CSYM,  CK_CSYM,  CK_CSYM,  CK_CSYM,  CK_CSYM,  CK_CSYM,  /* 4: @ABCDEFGHIJKLMNO */
	CK_CSYM,  CK_CSYM,  CK_CSYM,  CK_CSYM,  CK_CSYM,  CK_CSYM,  CK_CSYM,  CK_CSYM,  CK_CSYM,  CK_CSYM,  CK_CSYM,  CK_ETC,   CK_UDEF,  CK_ETC,   CK_ETC,   CK_CSYM,  /* 5: PQRSTUVWXYZ[\]^_ */
	CK_ETC,   CK_CSYM,  CK_CSYM,  CK_CSYM,  CK_CSYM,  CK_CSYM,  CK_CSYM,  CK_CSYM,  CK_CSYM,  CK_CSYM,  CK_CSYM,  CK_CSYM,  CK_CSYM,  CK_CSYM,  CK_CSYM,  CK_CSYM,  /* 6: `abcdefghijklmno */
	CK_CSYM,  CK_CSYM,  CK_CSYM,  CK_CSYM,  CK_CSYM,  CK_CSYM,  CK_CSYM,  CK_CSYM,  CK_CSYM,  CK_CSYM,  CK_CSYM,  CK_ETC,   CK_ETC,   CK_ETC,   CK_ETC,   CK_CTRL,   /* 7: pqrstuvwxyz{|}~. */
	/* 0: not-keyword, 1:__iscsym(), 2:user-define */
};

namespace WCODE
{
	static bool s_MultiFont;

	bool CalcHankakuByFont(wchar_t);

	//2007.08.30 kobake 追加
	bool IsHankaku(wchar_t wc, CCharWidthCache& cache)
	{
		//※ほぼ未検証。ロジックが確定したらインライン化すると良い。

		//参考：http://www.swanq.co.jp/blog/archives/000783.html
		if(
			   wc<=0x007E //ACODEとか
//			|| wc==0x00A5 //円マーク
//			|| wc==0x203E //にょろ
			|| (wc>=0xFF61 && wc<=0xFF9f)	// 半角カタカナ
		)return true;

		//0x7F ～ 0xA0 も半角とみなす
		//http://ja.wikipedia.org/wiki/Unicode%E4%B8%80%E8%A6%A7_0000-0FFF を見て、なんとなく
		if(wc>=0x007F && wc<=0x00A0)return true;	// Control Code ISO/IEC 6429

		// 漢字はすべて同一幅とみなす	// 2013.04.07 aroka
		if ( wc>=0x4E00 && wc<=0x9FBB		// Unified Ideographs, CJK
		  || wc>=0x3400 && wc<=0x4DB5		// Unified Ideographs Extension A, CJK
		){
			wc = 0x4E00; // '一'(0x4E00)の幅で代用
		}
		// ハングルはすべて同一幅とみなす	// 2013.04.08 aroka
		else if ( wc>=0xAC00 && wc<=0xD7A3 )		// Hangul Syllables
		{
			wc = 0xAC00; // (0xAC00)の幅で代用
		}
		// 外字はすべて同一幅とみなす	// 2013.04.08 aroka
		else if (wc>=0xE000 && wc<=0xE8FF) // Private Use Area
		{
			wc = 0xE000; // (0xE000)の幅で代用
		}

		//$$ 仮。もう動的に計算しちゃえ。(初回のみ)
		return cache.CalcHankakuByFont(wc);
	}

	//!制御文字であるかどうか
	[[nodiscard]] bool IsControlCode(wchar_t wc)
	{
		return wc < gm_keyword_char.size() && gm_keyword_char[wc] == CK_CTRL;
	}
}

void CCharWidthCache::DeleteLocalData()
{
	if (m_hFont != nullptr) {
		SelectObject(m_hdc, m_hFontOld);
		DeleteObject(m_hFont);
		m_hFont = nullptr;
	}
	if (m_hFontFull != nullptr) {
		SelectObject(m_hdcFull, m_hFontFullOld);
		DeleteObject(m_hFontFull);
		m_hFontFull = nullptr;
	}
	if(m_hdc){ DeleteDC(m_hdc); m_hdc = nullptr;}
	if(m_hdcFull){ DeleteDC(m_hdcFull);  m_hdcFull = nullptr;}
}

void CCharWidthCache::Reset(const LOGFONT &lf, const LOGFONT &lfFull, HDC hdcOrg)
{
	DeleteLocalData();

	m_hdc = ::CreateCompatibleDC(hdcOrg);
	m_lf = lf;
	m_lf2 = lfFull;

	m_hFont = ::CreateFontIndirect( &lf );
	m_hFontOld = (HFONT)SelectObject(m_hdc,m_hFont);
	const bool bFullFont = &lf != &lfFull && memcmp(&lf, &lfFull, sizeof(lf)) != 0;
	if( bFullFont ){
		m_bMultiFont = true;
		m_hdcFull = CreateCompatibleDC(hdcOrg);
		m_hFontFull = ::CreateFontIndirect(&lfFull);
		m_hFontFullOld = (HFONT)SelectObject(m_hdcFull, m_hFontFull);
	}else{
		m_bMultiFont = false;
		m_hdcFull = nullptr;
		m_hFontFull = nullptr;
		m_hFontFullOld = nullptr;
	}
	WCODE::s_MultiFont = m_bMultiFont;

	// -- -- 半角基準 -- -- //
	// CTextMetrics::Update と同じでなければならない
	std::array<HDC, 2> hdcArr = {m_hdc, m_hdcFull};
	int size = (bFullFont ? 2 : 1);
	m_han_size.cx = 1;
	m_han_size.cy = 1;
	for(int i = 0; i < size; i++){
		// KB145994
		// tmAveCharWidth は不正確(半角か全角なのかも不明な値を返す)
		SIZE sz;
		GetTextExtentPoint32(hdcArr[i], L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", 52, &sz);
		sz.cx = (sz.cx / 26 + 1) / 2;
		if( m_han_size.cx < sz.cx ){
			m_han_size.cx = sz.cx;
		}
		if( m_han_size.cy < sz.cy ){
			m_han_size.cy = sz.cy;
		}
	}

	std::copy(m_lf.lfFaceName, m_lf.lfFaceName + LF_FACESIZE, m_lfFaceName.begin());
	std::copy(m_lf2.lfFaceName, m_lf2.lfFaceName + LF_FACESIZE, m_lfFaceName2.begin());
	m_nCharPxWidthCache.fill(0);
}

bool CCharWidthCache::CalcHankakuByFont(wchar_t c)
{
	return CalcPxWidthByFont(c) <= m_han_size.cx;
}

int CCharWidthCache::QueryPixelWidth(wchar_t c) const
{
	SIZE size={m_han_size.cx*2,0}; //関数が失敗したときのことを考え、全角幅で初期化しておく
	// 2014.12.21 コントロールコードの表示・NULが1px幅になるのをスペース幅にする
	if (WCODE::IsControlCode(c)) {
		GetTextExtentPoint32(SelectHDC(c),&c,1,&size);
		const int nCx = size.cx;
		const wchar_t proxyChar = ((L'\0' == c) ? ' ' : L'･');
		GetTextExtentPoint32(SelectHDC(proxyChar),&proxyChar,1,&size);
		return t_max<int>(nCx, size.cx);
	}
	GetTextExtentPoint32(SelectHDC(c),&c,1,&size);
	return t_max<int>(1,size.cx);
}

int CCharWidthCache::CalcPxWidthByFont(wchar_t c) {
	// キャッシュから文字の情報を取得する。情報がなければ、計算して登録する。
	if (!m_nCharPxWidthCache[c]) {
		m_nCharPxWidthCache[c] = static_cast<short>(QueryPixelWidth(c));
	}
	return m_nCharPxWidthCache[c];
}

int CCharWidthCache::CalcPxWidthByFont2(const wchar_t* pc2) const
{
	SIZE size={m_han_size.cx*2,0};
	// サロゲートは全角フォント
	GetTextExtentPoint32(m_hdcFull?m_hdcFull:m_hdc,pc2,2,&size);
	return t_max<int>(1,size.cx);
}
		
[[nodiscard]] HDC CCharWidthCache::SelectHDC(wchar_t c) const
{
	return m_hdcFull && WCODE::GetFontNo(c) ? m_hdcFull : m_hdc;
}

namespace WCODE {
	// 文字の使用フォントを返す
	// @return 0:半角用 / 1:全角用
	[[nodiscard]] int GetFontNo( wchar_t c ){
		if (s_MultiFont && 0x0080 <= c && c <= 0xFFFF){
			return 1;
		}
		return 0;
	}
	[[nodiscard]] int GetFontNo2( wchar_t, wchar_t ){
		if( s_MultiFont ){
			return 1;
		}
		return 0;
	}
}

namespace {
std::array<CCharWidthCache, 3> currentCache;
ECharWidthFontMode currentMode = CWM_FONT_EDIT;
}

//	文字幅の動的計算用キャッシュの初期化。	2007/5/18 Uchi
void InitCharWidthCache( const LOGFONT &lf, ECharWidthFontMode fMode )
{
	HDC hdc = GetDC(nullptr);
	currentCache[fMode].Reset(lf, lf, hdc);
	ReleaseDC(nullptr, hdc);
}

void InitCharWidthCacheFromDC( const LOGFONT* lfs, ECharWidthFontMode fMode, HDC hdcOrg )
{
	currentCache[fMode].Reset(lfs[0], lfs[1], hdcOrg);
}

 //	文字幅の動的計算用キャッシュの選択	2013.04.08 aroka
void SelectCharWidthCache( ECharWidthFontMode fMode )
{
	currentMode = fMode;
	WCODE::s_MultiFont = currentCache[fMode].GetMultiFont();
}

[[nodiscard]] CCharWidthCache& GetCharWidthCache()
{
	return currentCache[currentMode];
}
