/*!	@file
	@brief IMEの処理

	@author Norio Nakatani
	@date	1998/03/13 作成
	@date   2008/04/13 CEditView.cppから分離
*/
/*
	Copyright (C) 1998-2002, Norio Nakatani
	Copyright (C) 2000, genta, JEPRO, MIK
	Copyright (C) 2001, genta, GAE, MIK, hor, asa-o, Stonee, Misaka, novice, YAZAKI
	Copyright (C) 2002, YAZAKI, hor, aroka, MIK, Moca, minfu, KK, novice, ai, Azumaiya, genta
	Copyright (C) 2003, MIK, ai, ryoji, Moca, wmlhq, genta
	Copyright (C) 2004, genta, Moca, novice, naoh, isearch, fotomo
	Copyright (C) 2005, genta, MIK, novice, aroka, D.S.Koba, かろと, Moca
	Copyright (C) 2006, Moca, aroka, ryoji, fon, genta
	Copyright (C) 2007, ryoji, じゅうじ, maru

	This source code is designed for sakura editor.
	Please contact the copyright holders to use this code for other purpose.
*/

#include "StdAfx.h"
#include "CEditView.h"
#include <algorithm>
#include "charset/CShiftJis.h"
#include "doc/CEditDoc.h"
#include "env/DLLSHAREDATA.h"
#include "_main/CAppMode.h"
#include "window/CEditWnd.h"

namespace {

class ImmContext {
public:
	ImmContext(HWND hwnd) : m_hwnd(hwnd), m_himc(ImmGetContext(hwnd)) {}
	~ImmContext() { if (m_himc) ImmReleaseContext(m_hwnd, m_himc); }
	operator HIMC() const { return m_himc; }
private:
	const HWND m_hwnd;
	const HIMC m_himc;
};

std::wstring GetCompositionString(HIMC imc, DWORD type) {
	const int requiredBytes = ImmGetCompositionString(imc, type, nullptr, 0);
	std::wstring string(requiredBytes / 2, L'\0');
	const int actualBytes = ImmGetCompositionString(imc, type, string.data(), requiredBytes);
	string.resize(actualBytes / 2);
	return string;
}

template <typename T>
std::vector<T> GetCompositionAttributes(HIMC imc, DWORD type) {
	const int requiredBytes = ImmGetCompositionString(imc, type, nullptr, 0);
	std::vector<T> buffer(requiredBytes / sizeof(T));
	ImmGetCompositionString(imc, type, buffer.data(), requiredBytes);
	return buffer;
}

}


// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
//                           IME                               //
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //

/*!	IME ONか

	@date  2006.12.04 ryoji 新規作成（関数化）
*/
bool CEditView::IsImeON( void )
{
	bool bRet;
	HIMC	hIme;
	DWORD	conv, sent;

	//	From here Nov. 26, 2006 genta
	hIme = ImmGetContext( GetHwnd() );
	if( ImmGetOpenStatus( hIme ) != FALSE ){
		ImmGetConversionStatus( hIme, &conv, &sent );
		if(( conv & IME_CMODE_NOCONVERSION ) == 0 ){
			bRet = true;
		}
		else {
			bRet = false;
		}
	}
	else {
		bRet = false;
	}
	ImmReleaseContext( GetHwnd(), hIme );
	//	To here Nov. 26, 2006 genta

	return bRet;
}

void CEditView::StartComposition()
{
	if (m_cViewSelect.IsTextSelected()) {
		GetCommander().HandleCommand(F_DELETE, false, 0, 0, 0, 0);
	}
	m_compositionLayoutRange.Set(GetCaret().GetCaretLayoutPos());
}

static bool IsTargetAttribute(const CompositionAttribute& attr) {
	return attr.kind == CompositionAttributeKind::TARGET_CONVERTED ||
		   attr.kind == CompositionAttributeKind::TARGET_NOTCONVERTED;
}

void CEditView::UpdateCompositionString(std::wstring_view text, int cursorPos,
	const std::vector<CompositionAttributeKind>& attrs, const std::vector<int>& clauses)
{
#ifdef _DEBUG
	std::wstring s(text);
	OutputDebugStringW(s.c_str());
#endif

	CLogicPoint logicFrom;
	m_pcEditDoc->m_cLayoutMgr.LayoutToLogic(m_compositionLayoutRange.GetFrom(), &logicFrom);

	// 属性データを内部形式に変換する
	m_compositionAttributes.clear();
	auto it = clauses.begin() + 1;  // 先頭は必ず0なので読み飛ばす
	CLogicInt logicFromX = logicFrom.GetX();
	CLogicInt logicToX;
	for (; it != clauses.end(); ++it) {
		logicToX = logicFrom.GetX() + *it;
		m_compositionAttributes.emplace_back(attrs[*it - 1], logicFromX, logicToX);
		logicFromX = logicToX;
	}

	// 行データ・レイアウト情報の更新
	ReplaceData_CEditView(m_compositionLayoutRange,
		text.data(), CLogicInt(text.size()), false, nullptr);

	// 新しい範囲情報を保存しておく
	m_pcEditDoc->m_cLayoutMgr.LogicToLayout(
		CLogicPoint(logicToX, logicFrom.GetY()), m_compositionLayoutRange.GetToPointer());

	// キャレットの位置を決める
	// IMMから通知されるカーソル位置と変換中の文字列の位置は必ずしも一致しない。現在変換中の文字列が
	// 画面外にスクロールされてしまうことがあるため、変換中文字列がある場合はその位置で上書きしておく。
	if (cursorPos == logicToX) {
		const auto attr = std::find_if(m_compositionAttributes.begin(),
			m_compositionAttributes.end(), IsTargetAttribute);
		if (attr != m_compositionAttributes.end()) {
			cursorPos = attr->start;
		}
	}

	// キャレットの移動と追従スクロール
	CLogicPoint caretLogicPos = logicFrom;
	caretLogicPos.Offset(cursorPos, 0);
	CLayoutPoint caretLayoutPos;
	m_pcEditDoc->m_cLayoutMgr.LogicToLayout(caretLogicPos, &caretLayoutPos);
	m_pcCaret->MoveCursor(caretLayoutPos, true);

	Call_OnPaint(PAINT_LINENUMBER | PAINT_BODY, false);

#ifdef _DEBUG
	wchar_t buffer[64];
	swprintf_s(buffer, L"\nCursor = %d, LayoutRange {(%d,%d), (%d,%d)}\n",
		cursorPos,
		m_compositionLayoutRange.GetFrom().GetX().GetValue(),
		m_compositionLayoutRange.GetFrom().GetY().GetValue(),
		m_compositionLayoutRange.GetTo().GetX().GetValue(),
		m_compositionLayoutRange.GetTo().GetY().GetValue()
	);
	OutputDebugStringW(buffer);
#endif
}

void CEditView::CompleteComposition(std::wstring_view text)
{
	ReplaceData_CEditView(m_compositionLayoutRange, L"", CLogicInt(0), false, nullptr);
	if (!IsInsMode()) {
		// 上書きモードなので挿入するものと同じ長さのテキストを先に消去しておく
		ReplaceData_CEditView(m_compositionLayoutRange, L"", CLogicInt(0), false, nullptr);
	}

	const CLayoutInt redrawTopLine = m_compositionLayoutRange.GetFrom().GetY();
	const CLayoutInt redrawBottomLine = m_compositionLayoutRange.GetTo().GetY();
	m_compositionLayoutRange.Clear(0);
	m_compositionAttributes.clear();

	/* テキストを貼り付け */
	if( m_bHideMouse && 0 <= m_nMousePouse ){
		m_nMousePouse = -1;
		::SetCursor( NULL );
	}
	BOOL bHokan = m_bHokan;

	GetCommander().HandleCommand( F_INSTEXT_W, false, (LPARAM)text.data(), (LPARAM)text.size(), TRUE, 0 );
	RedrawLines(redrawTopLine, redrawBottomLine);
	m_pcCaret->ShowEditCaret();

	m_bHokan = bHokan;	// 消されても表示中であるかのように誤魔化して入力補完を動作させる
	PostprocessCommand_hokan();	// 補完実行
}

void CEditView::CancelComposition()
{
	ReplaceData_CEditView(m_compositionLayoutRange, nullptr, CLogicInt(0), false, nullptr);
	m_pcCaret->MoveCursor(m_compositionLayoutRange.GetFrom(), true);
	m_compositionLayoutRange.Clear(0);
	m_compositionAttributes.clear();
	Call_OnPaint(PAINT_LINENUMBER | PAINT_BODY, false);
}

void CEditView::OnImeComposition(LPARAM lParam)
{
	if (!(lParam & 0x1fff)) {  // 入力操作のキャンセル通知
		CancelComposition();
		return;
	}
	else if (lParam & GCS_COMPSTR) {  // 編集中文字列の変更通知
		ImmContext imc(GetHwnd());
		const std::wstring text = GetCompositionString(imc, GCS_COMPSTR);
		const std::vector<CompositionAttributeKind> attrs =
			GetCompositionAttributes<CompositionAttributeKind>(imc, GCS_COMPATTR);
		const std::vector<int> clauses = GetCompositionAttributes<int>(imc, GCS_COMPCLAUSE);
		const int cursorPos = ImmGetCompositionString(imc, GCS_CURSORPOS, nullptr, 0);
		UpdateCompositionString(text, cursorPos, attrs, clauses);
		return;
	}
	else if (lParam & GCS_RESULTSTR) {  // 文字列の確定通知
		ImmContext imc(GetHwnd());
		CompleteComposition(GetCompositionString(imc, GCS_RESULTSTR));
		return;
	}
}

void CEditView::OnImeEndComposition()
{
	m_szComposition[0] = L'\0';
}

/* 再変換  by minfu 2002.03.27 */ // 20020331 aroka
LRESULT CEditView::OnImeRequest(WPARAM wParam, LPARAM lParam)
{
	// 2002.04.09 switch case に変更  minfu
	switch ( wParam ){
	case IMR_RECONVERTSTRING:
		return SetReconvertStruct((PRECONVERTSTRING)lParam, UNICODE_BOOL);

	case IMR_CONFIRMRECONVERTSTRING:
		return SetSelectionFromReonvert((PRECONVERTSTRING)lParam, UNICODE_BOOL);

	// 2010.03.16 MS-IME 2002 だと「カーソル位置の前後の内容を参照して変換を行う」の機能
	case IMR_DOCUMENTFEED:
		return SetReconvertStruct((PRECONVERTSTRING)lParam, UNICODE_BOOL, true);

	case IMR_QUERYCHARPOSITION: {
		// コンポジション文字列の描画位置の問い合わせ
		// pos->dwCharPos にIMEが問い合わせたい文字のインデックスが入っている。
		// 該当文字のスクリーン座標を pos->pt に入れて返す。
		IMECHARPOSITION* pos = reinterpret_cast<IMECHARPOSITION*>(lParam);
		pos->dwSize = sizeof(IMECHARPOSITION);
		pos->cLineHeight = m_cTextMetrics.GetHankakuDy();
		pos->rcDocument = m_pcTextArea->GetAreaRect();
		ClientToScreen(reinterpret_cast<POINT*>(&pos->rcDocument.left));
		ClientToScreen(reinterpret_cast<POINT*>(&pos->rcDocument.right));

		const POINT caretDrawPos = m_pcCaret->CalcCaretDrawPos(m_pcCaret->GetCaretLayoutPos());
		if (m_compositionLayoutRange.IsOne()) {
			pos->pt = caretDrawPos;
		} else {
			CLogicPoint logicFrom;
			m_pcEditDoc->m_cLayoutMgr.LayoutToLogic(
				m_compositionLayoutRange.GetFrom(), &logicFrom);
			CLogicInt logicCharPos = logicFrom.GetX() + CLogicInt(pos->dwCharPos);

			const auto end = m_compositionAttributes.end();
			const auto it = std::find_if(
				m_compositionAttributes.begin(), end,
				[logicCharPos](const CompositionAttribute& attr) {
					return attr.start <= logicCharPos && logicCharPos < attr.end;
				});
			pos->pt = (it != end) ? it->pos : caretDrawPos;
		}
		ClientToScreen(&pos->pt);
		return 1;
	}
	}
	// 2010.03.16 0LではなくTSFが何かするかもしれないのでDefにまかせる
	return ::DefWindowProc( GetHwnd(), WM_IME_REQUEST, wParam, lParam );
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
//                          再変換・変換補助
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
/*!
	@brief IMEの再変換/前後参照で、カーソル位置から前後200chars位を取り出してRECONVERTSTRINGを埋める
	@param  pReconv  [out]  RECONVERTSTRING構造体へのポインタ。NULLあり
	@param  bUnicode        trueならばUNICODEで構造体を埋める
	@param  bDocumentFeed   trueならばIMR_DOCUMENTFEEDとして処理する
	@return   RECONVERTSTRINGのサイズ。0ならIMEは何もしない(はず)
	@date 2002.04.09 minfu
	@date 2010.03.16 Moca IMR_DOCUMENTFEED対応
*/
LRESULT CEditView::SetReconvertStruct(PRECONVERTSTRING pReconv, bool bUnicode, bool bDocumentFeed)
{
	if( false == bDocumentFeed ){
		m_nLastReconvIndex = -1;
		m_nLastReconvLine  = -1;
	}
	
	//矩形選択中は何もしない
	if( GetSelectionInfo().IsBoxSelecting() )
		return 0;

	// 2010.04.06 ビューモードでは何もしない
	if( CAppMode::getInstance()->IsViewMode() ){
		return 0;
	}
	
	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
	//                      選択範囲を取得                         //
	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
	//選択範囲を取得 -> ptSelect, ptSelectTo, nSelectedLen
	CLogicPoint	ptSelect;
	CLogicPoint	ptSelectTo;
	int			nSelectedLen;
	if( GetSelectionInfo().IsTextSelected() ){
		//テキストが選択されているとき
		m_pcEditDoc->m_cLayoutMgr.LayoutToLogic(GetSelectionInfo().m_sSelect.GetFrom(), &ptSelect);
		m_pcEditDoc->m_cLayoutMgr.LayoutToLogic(GetSelectionInfo().m_sSelect.GetTo(), &ptSelectTo);
		
		// 選択範囲が複数行の時、１ロジック行以内に制限
		if (ptSelectTo.y != ptSelect.y){
			if( bDocumentFeed ){
				// 暫定：未選択として振舞う
				// 改善案：選択範囲は置換されるので、選択範囲の前後をIMEに渡す
				// ptSelectTo.y = ptSelectTo.y;
				ptSelectTo.x = ptSelect.x;
			}else{
				// 2010.04.06 対象をptSelect.yの行からカーソル行に変更
				const CDocLine* pDocLine = m_pcEditDoc->m_cDocLineMgr.GetLine(GetCaret().GetCaretLogicPos().y);
				CLogicInt targetY = GetCaret().GetCaretLogicPos().y;
				// カーソル行が実質無選択なら、直前・直後の行を選択
				if( ptSelect.y == GetCaret().GetCaretLogicPos().y
						&& pDocLine && pDocLine->GetLengthWithoutEOL() == GetCaret().GetCaretLogicPos().x ){
					// カーソルが上側行末 => 次の行。行末カーソルでのShift+Upなど
					targetY = t_min(m_pcEditDoc->m_cDocLineMgr.GetLineCount(),
						GetCaret().GetCaretLogicPos().y + 1);
					pDocLine = m_pcEditDoc->m_cDocLineMgr.GetLine(targetY);
				}else
				if( ptSelectTo.y == GetCaret().GetCaretLogicPos().y
						&& 0 == GetCaret().GetCaretLogicPos().x ){
					// カーソルが下側行頭 => 前の行。 行頭でShift+Down/Shift+End→Rightなど
					targetY = GetCaret().GetCaretLogicPos().y - 1;
					pDocLine = m_pcEditDoc->m_cDocLineMgr.GetLine(targetY);
				}
				// 選択範囲をxで指定：こちらはカーソルではなく選択範囲基準
				if(targetY == ptSelect.y){
					// ptSelect.x; 未変更
					ptSelectTo.x = pDocLine ? pDocLine->GetLengthWithoutEOL() : 0;
				}else
				if(targetY == ptSelectTo.y){
					ptSelect.x = 0;
					// ptSelectTo.x; 未変更
				}else{
					ptSelect.x = 0;
					ptSelectTo.x = pDocLine ? pDocLine->GetLengthWithoutEOL() : 0;
				}
				ptSelect.y = targetY;
				// ptSelectTo.y = targetY; 以下未使用
			}
		}
	}
	else{
		//テキストが選択されていないとき
		m_pcEditDoc->m_cLayoutMgr.LayoutToLogic(GetCaret().GetCaretLayoutPos(), &ptSelect);
		ptSelectTo = ptSelect;
	}
	nSelectedLen = ptSelectTo.x - ptSelect.x;
	// 以下 ptSelect.y ptSelect.x, nSelectedLen を使用

	//ドキュメント行取得 -> pcCurDocLine
	const CDocLine* pcCurDocLine = m_pcEditDoc->m_cDocLineMgr.GetLine(ptSelect.GetY2());
	if (NULL == pcCurDocLine )
		return 0;

	//テキスト取得 -> pLine, nLineLen
	const int nLineLen = pcCurDocLine->GetLengthWithoutEOL();
	if ( 0 == nLineLen )
		return 0;
	const wchar_t* pLine = pcCurDocLine->GetPtr();

	// 2010.04.17 行頭から←選択だと「SelectToが改行の後ろの位置」にあるため範囲を調整する
	// フリーカーソル選択でも行末より後ろにカーソルがある
	if( nLineLen < ptSelect.x ){
		// 改行直前をIMEに渡すカーソル位置ということにする
		ptSelect.x = CLogicInt(nLineLen);
		nSelectedLen = 0;
	}
	if( nLineLen <  ptSelect.x + nSelectedLen ){
		nSelectedLen = nLineLen - ptSelect.x;
	}

	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
	//              再変換範囲・考慮文字を修正                     //
	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //

	//再変換考慮文字列開始  //行の中で再変換のAPIにわたすとする文字列の開始位置
	int nReconvIndex = 0;
	int nInsertCompLen = 0; // DOCUMENTFEED用。変換中の文字列をdwStrに混ぜる
	// Iはカーソル　[]が選択範囲=dwTargetStrLenだとして
	// 行：日本語をIします。
	// IME：にゅうｒ
	// APIに渡す文字列：日本語を[にゅうｒ]Iします。

	// 選択開始位置より前後200(or 50)文字ずつを考慮文字列にする
	const int nReconvMaxLen = (bDocumentFeed ? 50 : 200); //$$マジックナンバー注意
	while (ptSelect.x - nReconvIndex > nReconvMaxLen) {
		nReconvIndex = t_max<int>(nReconvIndex+1, ::CharNext(pLine+nReconvIndex)-pLine);
	}
	
	//再変換考慮文字列終了  //行の中で再変換のAPIにわたすとする文字列の長さ
	int nReconvLen = nLineLen - nReconvIndex;
	if ( (nReconvLen + nReconvIndex - ptSelect.x) > nReconvMaxLen ){
		const wchar_t*       p = pLine + ptSelect.x;
		const wchar_t* const q = pLine + ptSelect.x + nReconvMaxLen;
		while (p <= q) {
			p = t_max(p+1, const_cast<const wchar_t*>(::CharNext(p)));
		}
		nReconvLen = p - pLine - nReconvIndex;
	}
	
	//対象文字列の調整
	if ( ptSelect.x + nSelectedLen > nReconvIndex + nReconvLen ){
		// 考慮分しかAPIに渡さないので、選択範囲を縮小
		nSelectedLen = nReconvLen + nReconvIndex - ptSelect.x;
	}
	
	if( bDocumentFeed ){
		// IMR_DOCUMENTFEEDでは、再変換対象はIMEから取得した入力中文字列
		nInsertCompLen = wcslen(m_szComposition);
		if( 0 == nInsertCompLen ){
			// 2回呼ばれるので、m_szCompositionに覚えておく
			HWND hwnd = GetHwnd();
			HIMC hIMC = ::ImmGetContext( hwnd );
			if( !hIMC ){
				return 0;
			}
			wmemset(m_szComposition, L'\0', _countof(m_szComposition));
			LONG immRet = ::ImmGetCompositionString(hIMC, GCS_COMPSTR, m_szComposition, _countof(m_szComposition));
			if( immRet == IMM_ERROR_NODATA || immRet == IMM_ERROR_GENERAL ){
				m_szComposition[0] = L'\0';
			}
			::ImmReleaseContext( hwnd, hIMC );
			nInsertCompLen = wcslen(m_szComposition);
		}
	}

	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
	//                      構造体設定要素                         //
	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //

	//行の中で再変換のAPIにわたすとする文字列の長さ
	int         cbReconvLenWithNull; // byte
	DWORD       dwReconvTextLen;    // CHARs
	DWORD       dwReconvTextInsLen; // CHARs
	DWORD       dwCompStrOffset;    // byte
	DWORD       dwCompStrLen;       // CHARs
	DWORD       dwInsByteCount = 0; // byte
	CNativeW    cmemBuf1;
	CNativeA    cmemBuf2;
	const void* pszReconv; 
	const void* pszInsBuffer;

	//UNICODE→UNICODE
	if(bUnicode){
		const WCHAR* pszCompInsStr = L"";
		int nCompInsStr   = 0;
		if( nInsertCompLen ){
			pszCompInsStr = m_szComposition;
			nCompInsStr   = wcslen( pszCompInsStr );
		}
		dwInsByteCount      = nCompInsStr * sizeof(wchar_t);
		dwReconvTextLen     = nReconvLen;
		dwReconvTextInsLen  = dwReconvTextLen + nCompInsStr;                 //reconv文字列長。文字単位。
		cbReconvLenWithNull = (dwReconvTextInsLen + 1) * sizeof(wchar_t);    //reconvデータ長。バイト単位。
		dwCompStrOffset     = (Int)(ptSelect.x - nReconvIndex) * sizeof(wchar_t);    //compオフセット。バイト単位。
		dwCompStrLen        = nSelectedLen + nCompInsStr;                            //comp文字列長。文字単位。
		pszReconv           = reinterpret_cast<const void*>(pLine + nReconvIndex);   //reconv文字列へのポインタ。
		pszInsBuffer        = pszCompInsStr;
	}
	//UNICODE→ANSI
	else{
		const wchar_t* pszReconvSrc =  pLine + nReconvIndex;

		//考慮文字列の開始から対象文字列の開始まで -> dwCompStrOffset
		if( ptSelect.x - nReconvIndex > 0 ){
			cmemBuf1.SetString(pszReconvSrc, ptSelect.x - nReconvIndex);
			CShiftJis::UnicodeToSJIS(cmemBuf1, cmemBuf2._GetMemory());
			dwCompStrOffset = cmemBuf2._GetMemory()->GetRawLength();				//compオフセット。バイト単位。
		}else{
			dwCompStrOffset = 0;
		}
		
		pszInsBuffer = "";
		//対象文字列の開始から対象文字列の終了まで -> dwCompStrLen
		if (nSelectedLen > 0 ){
			cmemBuf1.SetString(pszReconvSrc + ptSelect.x, nSelectedLen);
			CShiftJis::UnicodeToSJIS(cmemBuf1, cmemBuf2._GetMemory());
			dwCompStrLen = cmemBuf2._GetMemory()->GetRawLength();					//comp文字列長。文字単位。
		}else if(nInsertCompLen > 0){
			// nSelectedLen と nInsertCompLen が両方指定されることはないはず
			const ACHAR* pComp = to_achar(m_szComposition);
			pszInsBuffer = pComp;
			dwInsByteCount = strlen( pComp );
			dwCompStrLen = dwInsByteCount;
		}else{
			dwCompStrLen = 0;
		}
		
		//考慮文字列すべて
		cmemBuf1.SetString(pszReconvSrc , nReconvLen );
		CShiftJis::UnicodeToSJIS(cmemBuf1, cmemBuf2._GetMemory());
		
		dwReconvTextLen    = cmemBuf2._GetMemory()->GetRawLength();				//reconv文字列長。文字単位。
		dwReconvTextInsLen = dwReconvTextLen + dwInsByteCount;						//reconv文字列長。文字単位。
		cbReconvLenWithNull = cmemBuf2._GetMemory()->GetRawLength() + dwInsByteCount + sizeof(char);		//reconvデータ長。バイト単位。
		
		pszReconv = reinterpret_cast<const void*>(cmemBuf2._GetMemory()->GetRawPtr());	//reconv文字列へのポインタ
	}
	
	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
	//                        構造体設定                           //
	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
	if ( NULL != pReconv) {
		//再変換構造体の設定
		DWORD dwOrgSize = pReconv->dwSize;
		// 2010.03.17 Moca dwSizeはpReconvを用意する側(IME等)が設定
		//     のはずなのに Win XP+IME2002+TSF では dwSizeが0で送られてくる
		if( dwOrgSize != 0 && dwOrgSize < sizeof(*pReconv) + cbReconvLenWithNull ){
			// バッファ不足
			m_szComposition[0] = L'\0';
			return 0;
		}
		else if( 0 == dwOrgSize ){
			pReconv->dwSize = sizeof(*pReconv) + cbReconvLenWithNull;
		}
		pReconv->dwVersion         = 0;
		pReconv->dwStrLen          = dwReconvTextInsLen;	//文字単位
		pReconv->dwStrOffset       = sizeof(*pReconv) ;
		pReconv->dwCompStrLen      = dwCompStrLen;		//文字単位
		pReconv->dwCompStrOffset   = dwCompStrOffset;	//バイト単位
		pReconv->dwTargetStrLen    = dwCompStrLen;		//文字単位
		pReconv->dwTargetStrOffset = dwCompStrOffset;	//バイト単位
		
		// 2004.01.28 Moca ヌル終端の修正
		if( bUnicode ){
			WCHAR* p = (WCHAR*)(pReconv + 1);
			if( dwInsByteCount ){
				// カーソル位置に、入力中IMEデータを挿入
				CHAR* pb = (CHAR*)p;
				CopyMemory(pb, pszReconv, dwCompStrOffset);
				pb += dwCompStrOffset;
				CopyMemory(pb, pszInsBuffer, dwInsByteCount);
				pb += dwInsByteCount;
				CopyMemory(pb, ((char*)pszReconv) + dwCompStrOffset,
					dwReconvTextLen*sizeof(wchar_t) - dwCompStrOffset);
			}else{
				CopyMemory(p, pszReconv, cbReconvLenWithNull - sizeof(wchar_t));
			}
			// \0があると応答なしになることがある
			for( DWORD i = 0; i < dwReconvTextInsLen; i++ ){
				if( p[i] == 0 ){
					p[i] = L' ';
				}
			}
			p[dwReconvTextInsLen] = L'\0';
		}else{
			ACHAR* p = (ACHAR*)(pReconv + 1);
			if( dwInsByteCount ){
				CHAR* pb = p;
				CopyMemory(p, pszReconv, dwCompStrOffset);
				pb += dwCompStrOffset;
				CopyMemory(pb, pszInsBuffer, dwInsByteCount);
				pb += dwInsByteCount;
				CopyMemory(pb, ((char*)pszReconv) + dwCompStrOffset,
					dwReconvTextLen - dwCompStrOffset);
			}else{
				CopyMemory(p, pszReconv, cbReconvLenWithNull - sizeof(char));
			}
			// \0があると応答なしになることがある
			for( DWORD i = 0; i < dwReconvTextInsLen; i++ ){
				if( p[i] == 0 ){
					p[i] = ' ';
				}
			}
			p[dwReconvTextInsLen]='\0';
		}
	}
	
	if( false == bDocumentFeed ){
		// 再変換情報の保存
		m_nLastReconvIndex = nReconvIndex;
		m_nLastReconvLine  = ptSelect.y;
	}
	if( bDocumentFeed && pReconv ){
		m_szComposition[0] = L'\0';
	}
	return sizeof(RECONVERTSTRING) + cbReconvLenWithNull;
}

/*再変換用 エディタ上の選択範囲を変更する 2002.04.09 minfu */
LRESULT CEditView::SetSelectionFromReonvert(const PRECONVERTSTRING pReconv, bool bUnicode){
	
	// 再変換情報が保存されているか
	if ( (m_nLastReconvIndex < 0) || (m_nLastReconvLine < 0))
		return 0;

	if ( GetSelectionInfo().IsTextSelected()) 
		GetSelectionInfo().DisableSelectArea( true );

	if( 0 != pReconv->dwVersion ){
		return 0;
	}
	
	DWORD dwOffset, dwLen;

	//UNICODE→UNICODE
	if(bUnicode){
		dwOffset = pReconv->dwCompStrOffset/sizeof(WCHAR);	//0またはデータ長。バイト単位。→文字単位
		dwLen    = pReconv->dwCompStrLen;					//0または文字列長。文字単位。
	}
	//ANSI→UNICODE
	else{
		CNativeW	cmemBuf;

		//考慮文字列の開始から対象文字列の開始まで
		if( pReconv->dwCompStrOffset > 0){
			if( pReconv->dwSize < (pReconv->dwStrOffset + pReconv->dwCompStrOffset) ){
				return 0;
			}
			// 2010.03.17 sizeof(pReconv)+1ではなくdwStrOffsetを利用するように
			const char* p=((const char*)(pReconv)) + pReconv->dwStrOffset;
			cmemBuf._GetMemory()->SetRawData(p, pReconv->dwCompStrOffset );
			CShiftJis::SJISToUnicode(*(cmemBuf._GetMemory()), &cmemBuf);
			dwOffset = cmemBuf.GetStringLength();
		}else{
			dwOffset = 0;
		}

		//対象文字列の開始から対象文字列の終了まで
		if( pReconv->dwCompStrLen > 0 ){
			if( pReconv->dwSize <
					pReconv->dwStrOffset + pReconv->dwCompStrOffset + pReconv->dwCompStrLen*sizeof(char) ){
				return 0;
			}
			// 2010.03.17 sizeof(pReconv)+1ではなくdwStrOffsetを利用するように
			const char* p= ((const char*)pReconv) + pReconv->dwStrOffset;
			cmemBuf._GetMemory()->SetRawData(p + pReconv->dwCompStrOffset, pReconv->dwCompStrLen);
			CShiftJis::SJISToUnicode(*(cmemBuf._GetMemory()), &cmemBuf);
			dwLen = cmemBuf.GetStringLength();
		}else{
			dwLen = 0;
		}
	}
	
	//選択開始の位置を取得
	m_pcEditDoc->m_cLayoutMgr.LogicToLayout(
		CLogicPoint(m_nLastReconvIndex + dwOffset, m_nLastReconvLine),
		GetSelectionInfo().m_sSelect.GetFromPointer()
	);

	//選択終了の位置を取得
	m_pcEditDoc->m_cLayoutMgr.LogicToLayout(
		CLogicPoint(m_nLastReconvIndex + dwOffset + dwLen, m_nLastReconvLine),
		GetSelectionInfo().m_sSelect.GetToPointer()
	);

	// 単語の先頭にカーソルを移動
	GetCaret().MoveCursor( GetSelectionInfo().m_sSelect.GetFrom(), true );

	//選択範囲再描画 
	GetSelectionInfo().DrawSelectArea();

	// 再変換情報の破棄
	m_nLastReconvIndex = -1;
	m_nLastReconvLine  = -1;

	return 1;
}
