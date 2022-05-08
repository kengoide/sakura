﻿/*! @file */
/*
	Copyright (C) 2008, kobake
	Copyright (C) 2018-2022, Sakura Editor Organization

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
#ifndef SAKURA_CCLIPBOARD_4E783022_214C_4E51_A2E0_54EC343500F6_H_
#define SAKURA_CCLIPBOARD_4E783022_214C_4E51_A2E0_54EC343500F6_H_
#pragma once

#include <optional>
#include <Windows.h>

class CEol;
class CNativeW;
class CStringRef;

//!サクラエディタ用クリップボードクラス。後々はこの中で全てのクリップボードAPIを呼ばせたい。
class CClipboard{
	using Me = CClipboard;

	CClipboard(const Me&) = delete;
	Me& operator = (const Me&) = delete;

public:
	CClipboard(Me&&) noexcept = default;
	Me& operator = (Me&&) noexcept = default;
	virtual ~CClipboard();

	//! クリップボードを開く。
	//! @retval CClipboard クリップボードのオープンに成功した。
	//! @retval std::nullopt クリップボードのオープンに失敗した。
	static std::optional<CClipboard> Open(HWND hWnd);
	void Empty(); //!< クリップボードを空にする
	bool SetText(const wchar_t* pData, int nDataLen, bool bColumnSelect, bool bLineSelect, UINT uFormat = (UINT)-1);   //!< テキストを設定する
	bool SetHtmlText(const CNativeW& cmemBUf);
	bool GetText(CNativeW* cmemBuf, bool* pbColumnSelect, bool* pbLineSelect, const CEol& cEol, UINT uGetFormat = (UINT)-1); //!< テキストを取得する
	bool IsIncludeClipboradFormat(const wchar_t* pFormatName);
	bool SetClipboradByFormat(const CStringRef& cstr, const wchar_t* pFormatName, int nMode, int nEndMode);
	bool GetClipboradByFormat(CNativeW& mem, const wchar_t* pFormatName, int nMode, int nEndMode, const CEol& cEol);

private:
	int GetDataType() const; //!< クリップボードデータ形式(CF_UNICODETEXT等)の取得

public:
	// -- -- staticインターフェース -- -- //
	static bool HasValidData();    //!< クリップボード内に、サクラエディタで扱えるデータがあればtrue
	static CLIPFORMAT GetSakuraFormat(); //!< サクラエディタ独自のクリップボードデータ形式

protected:
	// 単体テスト用に protected で宣言する。
	CClipboard() = default;

	// 同名の Windows API に引数を転送する仮想メンバ関数。
	// 単体テスト内でオーバーライドすることで副作用のないテストを実施するのが目的。
	virtual HANDLE SetClipboardData(UINT uFormat, HANDLE hMem) const;
	virtual HANDLE GetClipboardData(UINT uFormat) const;
	virtual BOOL EmptyClipboard() const;
	virtual BOOL IsClipboardFormatAvailable(UINT format) const;
	virtual UINT EnumClipboardFormats(UINT format) const;
	virtual HGLOBAL GlobalAlloc(UINT uFlags, SIZE_T dwBytes) const;
	virtual LPVOID GlobalLock(HGLOBAL hMem) const;
};
#endif /* SAKURA_CCLIPBOARD_4E783022_214C_4E51_A2E0_54EC343500F6_H_ */
