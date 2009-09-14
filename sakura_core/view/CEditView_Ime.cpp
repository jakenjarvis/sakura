#include "stdafx.h"
#include "CEditView.h"
#include "charset/CShiftJis.h"


// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
//                           IME                               //
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //


/*!	IME ON��

	@date  2006.12.04 ryoji �V�K�쐬�i�֐����j
*/
bool CEditView::IsImeON( void )
{
	bool bRet;
	HIMC	hIme;
	DWORD	conv, sent;

	//	From here Nov. 26, 2006 genta
	hIme = ImmGetContext( m_hwndParent );
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
	ImmReleaseContext( m_hwndParent, hIme );
	//	To here Nov. 26, 2006 genta

	return bRet;
}

/* IME�ҏW�G���A�̈ʒu��ύX */
void CEditView::SetIMECompFormPos( void )
{
	//
	// If current composition form mode is near caret operation,
	// application should inform IME UI the caret position has been
	// changed. IME UI will make decision whether it has to adjust
	// composition window position.
	//
	//
	COMPOSITIONFORM	CompForm;
	HIMC			hIMC = ::ImmGetContext( GetHwnd() );
	POINT			point;
	HWND			hwndFrame;
	hwndFrame = ::GetParent( m_hwndParent );

	::GetCaretPos( &point );
	CompForm.dwStyle = CFS_POINT;
	CompForm.ptCurrentPos.x = (long) point.x;
	CompForm.ptCurrentPos.y = (long) point.y + GetCaret().GetCaretSize().cy - GetTextMetrics().GetHankakuHeight();

	if ( hIMC ){
		::ImmSetCompositionWindow( hIMC, &CompForm );
	}
	::ImmReleaseContext( GetHwnd() , hIMC );
}


/* IME�ҏW�G���A�̕\���t�H���g��ύX */
void CEditView::SetIMECompFormFont( void )
{
	//
	// If current composition form mode is near caret operation,
	// application should inform IME UI the caret position has been
	// changed. IME UI will make decision whether it has to adjust
	// composition window position.
	//
	//
	HIMC	hIMC = ::ImmGetContext( GetHwnd() );
	if ( hIMC ){
		::ImmSetCompositionFont( hIMC, &(GetDllShareData().m_Common.m_sView.m_lf) );
	}
	::ImmReleaseContext( GetHwnd() , hIMC );
}


// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
//                          �ĕϊ�                             //
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //

//  2002.04.09 minfu from here
/*�ĕϊ��p �J�[�\���ʒu����O��200byte�����o����RECONVERTSTRING�𖄂߂� */
/*  ����  pReconv RECONVERTSTRING�\���̂ւ̃|�C���^�B                     */
/*        bUnicode true�Ȃ��UNICODE�ō\���̂𖄂߂�                      */
/*  �߂�l   RECONVERTSTRING�̃T�C�Y                                      */
LRESULT CEditView::SetReconvertStruct(PRECONVERTSTRING pReconv, bool bUnicode)
{
	m_nLastReconvIndex = -1;
	m_nLastReconvLine  = -1;
	
	//��`�I�𒆂͉������Ȃ�
	if( GetSelectionInfo().IsBoxSelecting() )
		return 0;
	
	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
	//                      �I��͈͂��擾                         //
	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
	//�I��͈͂��擾 -> ptSelect, ptSelectTo, nSelectedLen
	CLogicPoint	ptSelect;
	CLogicPoint	ptSelectTo;
	int			nSelectedLen;
	if( GetSelectionInfo().IsTextSelected() ){
		//�e�L�X�g���I������Ă���Ƃ�
		m_pcEditDoc->m_cLayoutMgr.LayoutToLogic(GetSelectionInfo().m_sSelect.GetFrom(), &ptSelect);
		m_pcEditDoc->m_cLayoutMgr.LayoutToLogic(GetSelectionInfo().m_sSelect.GetTo(), &ptSelectTo);
		
		//�I��͈͂������s�̎���
		if (ptSelectTo.y != ptSelect.y){
			//�s���܂łɐ���
			CDocLine* pDocLine = m_pcEditDoc->m_cDocLineMgr.GetLine(ptSelect.GetY2());
			ptSelectTo.x = pDocLine->GetLengthWithEOL();
		}
	}
	else{
		//�e�L�X�g���I������Ă��Ȃ��Ƃ�
		m_pcEditDoc->m_cLayoutMgr.LayoutToLogic(GetCaret().GetCaretLayoutPos(), &ptSelect);
		ptSelectTo = ptSelect;
	}
	nSelectedLen = ptSelectTo.x - ptSelect.x;

	//�h�L�������g�s�擾 -> pcCurDocLine
	CDocLine* pcCurDocLine = m_pcEditDoc->m_cDocLineMgr.GetLine(ptSelect.GetY2());
	if (NULL == pcCurDocLine )
		return 0;

	//�e�L�X�g�擾 -> pLine, nLineLen
	int nLineLen = pcCurDocLine->GetLengthWithEOL() - pcCurDocLine->GetEol().GetLen() ; //���s�R�[�h���̂���������
	if ( 0 == nLineLen )
		return 0;
	const wchar_t* pLine = pcCurDocLine->GetPtr();


	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
	//                      �I��͈͂��C��                         //
	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //

	//�ĕϊ��l��������J�n  //�s�̒��ōĕϊ���API�ɂ킽���Ƃ��镶����̊J�n�ʒu
	int nReconvIndex = 0;
	if ( ptSelect.x > 200 ) { //$$�}�W�b�N�i���o�[����
		const wchar_t* pszWork = pLine;
		while( (ptSelect.x - nReconvIndex) > 200 ){
			pszWork = ::CharNextW_AnyBuild( pszWork);
			nReconvIndex = pszWork - pLine ;
		}
	}
	
	//�ĕϊ��l��������I��  //�s�̒��ōĕϊ���API�ɂ킽���Ƃ��镶����̒���
	int nReconvLen = nLineLen - nReconvIndex;
	if ( (nReconvLen + nReconvIndex - ptSelect.x) > 200 ){
		const wchar_t* pszWork = pLine + ptSelect.x;
		nReconvLen = ptSelect.x - nReconvIndex;
		while( ( nReconvLen + nReconvIndex - ptSelect.x) <= 200 ){
			pszWork = ::CharNextW_AnyBuild( pszWork);
			nReconvLen = pszWork - (pLine + nReconvIndex) ;
		}
	}
	
	//�Ώە�����̒���
	if ( ptSelect.x + nSelectedLen > nReconvIndex + nReconvLen ){
		nSelectedLen = nReconvIndex + nReconvLen - ptSelect.x;
	}
	

	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
	//                      �\���̐ݒ�v�f                         //
	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //

	//�s�̒��ōĕϊ���API�ɂ킽���Ƃ��镶����̒���
	int			nReconvLenWithNull;
	DWORD		dwReconvTextLen;
	DWORD		dwCompStrOffset, dwCompStrLen;
	CNativeW	cmemBuf1;
	const void*	pszReconv;

	//UNICODE��UNICODE
	if(bUnicode){
		dwReconvTextLen    = nReconvLen;											//reconv�����񒷁B�����P�ʁB
		nReconvLenWithNull = (nReconvLen + 1) * sizeof(wchar_t);					//reconv�f�[�^���B�o�C�g�P�ʁB
		dwCompStrOffset    = (Int)(ptSelect.x - nReconvIndex) * sizeof(wchar_t);	//comp�I�t�Z�b�g�B�o�C�g�P�ʁB
		dwCompStrLen       = nSelectedLen;											//comp�����񒷁B�����P�ʁB
		pszReconv          = reinterpret_cast<const void*>(pLine + nReconvIndex);	//reconv������ւ̃|�C���^�B
	}
	//UNICODE��ANSI
	else{
		const wchar_t* pszReconvSrc =  pLine + nReconvIndex;

		//�l��������̊J�n����Ώە�����̊J�n�܂� -> dwCompStrOffset
		if( ptSelect.x - nReconvIndex > 0 ){
			cmemBuf1.SetString(pszReconvSrc, ptSelect.x - nReconvIndex);
			CShiftJis::UnicodeToSJIS(cmemBuf1._GetMemory());
			dwCompStrOffset = cmemBuf1._GetMemory()->GetRawLength();				//comp�I�t�Z�b�g�B�o�C�g�P�ʁB
		}else{
			dwCompStrOffset = 0;
		}
		
		//�Ώە�����̊J�n����Ώە�����̏I���܂� -> dwCompStrLen
		if (nSelectedLen > 0 ){
			cmemBuf1.SetString(pszReconvSrc + ptSelect.x, nSelectedLen);  
			CShiftJis::UnicodeToSJIS(cmemBuf1._GetMemory());
			dwCompStrLen = cmemBuf1._GetMemory()->GetRawLength();					//comp�����񒷁B�����P�ʁB
		}else{
			dwCompStrLen = 0;
		}
		
		//�l�������񂷂ׂ�
		cmemBuf1.SetString(pszReconvSrc , nReconvLen );
		CShiftJis::UnicodeToSJIS(cmemBuf1._GetMemory());
		
		dwReconvTextLen =  cmemBuf1._GetMemory()->GetRawLength();						//reconv�����񒷁B�����P�ʁB
		nReconvLenWithNull =  cmemBuf1._GetMemory()->GetRawLength() + sizeof(char);		//reconv�f�[�^���B�o�C�g�P�ʁB
		
		pszReconv = reinterpret_cast<const void*>(cmemBuf1._GetMemory()->GetRawPtr());	//reconv������ւ̃|�C���^
	}
	
	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
	//                        �\���̐ݒ�                           //
	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
	if ( NULL != pReconv) {
		//�ĕϊ��\���̂̐ݒ�
		pReconv->dwSize            = sizeof(*pReconv) + nReconvLenWithNull ;
		pReconv->dwVersion         = 0;
		pReconv->dwStrLen          = dwReconvTextLen ;	//�����P��
		pReconv->dwStrOffset       = sizeof(*pReconv) ;
		pReconv->dwCompStrLen      = dwCompStrLen;		//�����P��
		pReconv->dwCompStrOffset   = dwCompStrOffset;	//�o�C�g�P��
		pReconv->dwTargetStrLen    = dwCompStrLen;		//�����P��
		pReconv->dwTargetStrOffset = dwCompStrOffset;	//�o�C�g�P��
		
		// 2004.01.28 Moca �k���I�[�̏C��
		if( bUnicode ){
			WCHAR* p = (WCHAR*)(pReconv + 1);
			CopyMemory(p, pszReconv, nReconvLenWithNull - sizeof(wchar_t));
			p[dwReconvTextLen] = L'\0';
		}else{
			ACHAR* p = (ACHAR*)(pReconv + 1);
			CopyMemory(p, pszReconv, nReconvLenWithNull - sizeof(char));
			p[dwReconvTextLen]='\0';
		}
	}
	
	// �ĕϊ����̕ۑ�
	m_nLastReconvIndex = nReconvIndex;
	m_nLastReconvLine  = ptSelect.y;
	
	return sizeof(RECONVERTSTRING) + nReconvLenWithNull;

}

/*�ĕϊ��p �G�f�B�^��̑I��͈͂�ύX���� 2002.04.09 minfu */
LRESULT CEditView::SetSelectionFromReonvert(const PRECONVERTSTRING pReconv, bool bUnicode){
	
	// �ĕϊ���񂪕ۑ�����Ă��邩
	if ( (m_nLastReconvIndex < 0) || (m_nLastReconvLine < 0))
		return 0;

	if ( GetSelectionInfo().IsTextSelected()) 
		GetSelectionInfo().DisableSelectArea( TRUE );

	DWORD dwOffset, dwLen;

	//UNICODE��UNICODE
	if(bUnicode){
		dwOffset = pReconv->dwCompStrOffset/sizeof(WCHAR);	//0�܂��̓f�[�^���B�o�C�g�P�ʁB�������P��
		dwLen    = pReconv->dwCompStrLen;					//0�܂��͕����񒷁B�����P�ʁB
	}
	//ANSI��UNICODE
	else{
		CNativeA	cmemBuf;

		//�l��������̊J�n����Ώە�����̊J�n�܂�
		if( pReconv->dwCompStrOffset > 0){
			const char* p=(const char*)(pReconv+1);
			cmemBuf.SetString(p, pReconv->dwCompStrOffset ); 
			CShiftJis::SJISToUnicode(cmemBuf._GetMemory());
			dwOffset = cmemBuf._GetMemory()->GetRawLength()/sizeof(WCHAR);
		}else{
			dwOffset = 0;
		}

		//�Ώە�����̊J�n����Ώە�����̏I���܂�
		if( pReconv->dwCompStrLen > 0 ){
			const char* p=(const char*)(pReconv+1);
			cmemBuf.SetString(p + pReconv->dwCompStrOffset, pReconv->dwCompStrLen); 
			CShiftJis::SJISToUnicode(cmemBuf._GetMemory());
			dwLen = cmemBuf._GetMemory()->GetRawLength()/sizeof(WCHAR);
		}else{
			dwLen = 0;
		}
	}
	
	//�I���J�n�̈ʒu���擾
	m_pcEditDoc->m_cLayoutMgr.LogicToLayout(
		CLogicPoint(m_nLastReconvIndex + dwOffset, m_nLastReconvLine),
		GetSelectionInfo().m_sSelect.GetFromPointer()
	);

	//�I���I���̈ʒu���擾
	m_pcEditDoc->m_cLayoutMgr.LogicToLayout(
		CLogicPoint(m_nLastReconvIndex + dwOffset + dwLen, m_nLastReconvLine),
		GetSelectionInfo().m_sSelect.GetToPointer()
	);

	// �P��̐擪�ɃJ�[�\�����ړ�
	GetCaret().MoveCursor( GetSelectionInfo().m_sSelect.GetFrom(), TRUE );

	//�I��͈͍ĕ`�� 
	GetSelectionInfo().DrawSelectArea();

	// �ĕϊ����̔j��
	m_nLastReconvIndex = -1;
	m_nLastReconvLine  = -1;

	return 1;

}