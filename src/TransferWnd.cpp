// This file is part of the aMule Project
//
// Copyright (c) 2003-2004 aMule Project ( http://www.amule-project.net )
// Copyright (C) 2002 Merkur ( merkur-@users.sourceforge.net / http://www.emule-project.net )
//
//This program is free software; you can redistribute it and/or
//modify it under the terms of the GNU General Public License
//as published by the Free Software Foundation; either
//version 2 of the License, or (at your option) any later version.
//
//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//GNU General Public License for more details.
//
//You should have received a copy of the GNU General Public License
//along with this program; if not, write to the Free Software
//Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.


// TransferWnd.cpp : implementation file

#include <wx/settings.h>
#include <wx/splitter.h>
#include <wx/defs.h>		// Needed before any other wx/*.h
#include <wx/sizer.h>		// Needed for wxSizer

#include "TransferWnd.h"	// Interface declarations
#include "amuleDlg.h"		// Needed for CamuleDlg
#include "PartFile.h"		// Needed for PR_LOW
#include "DownloadQueue.h"	// Needed for CDownloadQueue
#include "CatDialog.h"		// Needed for CCatDialog
#include "opcodes.h"		// Needed for MP_CAT_SET0
#include "DownloadListCtrl.h"	// Needed for CDownloadListCtrl
#include "UploadListCtrl.h"	// Needed for CUploadListCtrl
#include "UploadQueue.h"	// Needed for CUploadQueue
#include "otherfunctions.h"	// Needed for GetCatTitle
#include "amule.h"			// Needed for theApp
#include "QueueListCtrl.h"	// Needed for CQueueListCtrl
#include "muuli_wdr.h"		// Needed for ID_CATEGORIES
#include "SearchDlg.h"		// Needed for CSearchDlg->UpdateCatChoice()
#include "MuleNotebook.h"

// CTransferWnd dialog

IMPLEMENT_DYNAMIC_CLASS(CTransferWnd,wxPanel)

BEGIN_EVENT_TABLE(CTransferWnd,wxPanel)
	EVT_NOTEBOOK_PAGE_CHANGED(ID_CATEGORIES,CTransferWnd::OnSelchangeDltab)
	EVT_RIGHT_DOWN(CTransferWnd::OnNMRclickDLtab)
	EVT_SPLITTER_SASH_POS_CHANGED(ID_SPLATTER, CTransferWnd::OnSashPositionChanged)
	EVT_BUTTON(ID_BTNCLRCOMPL, CTransferWnd::OnBtnClearDownloads)
	EVT_BUTTON(ID_BTNSWITCHUP, CTransferWnd::OnBtnSwitchUpload)
END_EVENT_TABLE()


//IMPLEMENT_DYNAMIC(CTransferWnd, CDialog)
CTransferWnd::CTransferWnd(wxWindow* pParent /*=NULL*/)
: wxPanel(pParent,CTransferWnd::IDD)
{
	wxSizer* content=transferDlg(this,TRUE);
	content->Show(this,TRUE);

	uploadlistctrl=(CUploadListCtrl*)FindWindowByName(wxT("uploadList"));
	downloadlistctrl=(CDownloadListCtrl*)FindWindowByName(wxT("downloadList"));
	queuelistctrl=(CQueueListCtrl*)FindWindowByName(wxT("uploadQueue"));
	// let's hide the queue
	queueSizer->Remove(queuelistctrl);
	Layout();
	queuelistctrl->Show(FALSE);
	// allow notebook to dispatch right mouse clicks to us
	CMuleNotebook* nb=(CMuleNotebook*)FindWindowById(ID_CATEGORIES);
	nb->SetMouseListener(GetEventHandler());
	windowtransferstate=false;
	CatMenu=false;
}

CTransferWnd::~CTransferWnd()
{
}

bool CTransferWnd::OnInitDialog()
{
	m_dlTab=(CMuleNotebook*)FindWindowById(ID_CATEGORIES);

	// show & cat-tabs
	theApp.glob_prefs->GetCategory(0)->title = GetCatTitle(theApp.glob_prefs->GetAllcatType());

	theApp.glob_prefs->GetCategory(0)->incomingpath = char2unicode(theApp.glob_prefs->GetIncomingDir());
	
	for (uint32 ix=0;ix<theApp.glob_prefs->GetCatCount();ix++) {
		wxPanel* nullPanel=new wxPanel(m_dlTab);
		m_dlTab->AddPage(nullPanel,wxT("-")); // just temporary string.
		// for some odd reason, wxwin2.5 and gtk2 will not allow non utf strings for AddPage()
		// but they will be accepted in SetPageText().. so let's use this as a countermeasure
		m_dlTab->SetPageText(ix,theApp.glob_prefs->GetCategory(ix)->title);
	}
	
	return true;
}

void CTransferWnd::ShowQueueCount(uint32 number)
{
	char buffer[100];
	wxString fmtstr=wxT("%u (%u ")+ wxString(_("Banned")).MakeLower() +wxT(")");
	sprintf(buffer,unicode2char(fmtstr),number,theApp.uploadqueue->GetBanCount());
	wxStaticCast(FindWindowByName(wxT("clientCount")),wxStaticText)->SetLabel(char2unicode(buffer));
	//this->GetDlgItem(IDC_QUEUECOUNT)->SetWindowText(buffer);
}

void CTransferWnd::SwitchUploadList(wxCommandEvent& evt)
{
	if( windowtransferstate == false) {
		windowtransferstate=true;
		// hide the upload list
		queueSizer->Remove(uploadlistctrl);
		uploadlistctrl->Show(FALSE);
		queueSizer->Add(queuelistctrl,1,wxGROW|wxALIGN_CENTER_VERTICAL ,5);
		queuelistctrl->Show();
		queueSizer->Layout();
		wxStaticCast(FindWindowByName(wxT("uploadTitle")),wxStaticText)->SetLabel(_("On Queue"));
	} else {
		windowtransferstate=false;
		// hide the queuelist
		queueSizer->Remove(queuelistctrl);
		queuelistctrl->Show(FALSE);
		queueSizer->Add(uploadlistctrl,1,wxGROW|wxALIGN_CENTER_VERTICAL, 5);
		uploadlistctrl->Show();
		queueSizer->Layout();
		wxStaticCast(FindWindowByName(wxT("uploadTitle")),wxStaticText)->SetLabel(_("Uploads"));
	}
}

void CTransferWnd::Localize()
{
}


void CTransferWnd::OnSelchangeDltab(wxNotebookEvent& evt)
{
  downloadlistctrl->ChangeCategory(evt.GetSelection());
  downloadlistctrl->InitSort();
}

void CTransferWnd::OnNMRclickDLtab(wxMouseEvent& evt) {
	CMuleNotebook* nb=(CMuleNotebook*)FindWindowById(ID_CATEGORIES);
	if(nb->GetSelection()==-1) {
		return;
	}
	
	// Avoid opening another menu when it's already open
	if (CatMenu==false) {  
		
		CatMenu=true;
		wxMenu* menu=new wxMenu(_("Category"));

		if(nb->GetSelection()==0) {
			wxMenu* m_CatMenu=new wxMenu();

			m_CatMenu->Append(MP_CAT_SET0,_("all"));
			m_CatMenu->Append(MP_CAT_SET0+1,_("all others"));
			m_CatMenu->AppendSeparator();
			m_CatMenu->Append(MP_CAT_SET0+2,_("Incomplete"));
			m_CatMenu->Append(MP_CAT_SET0+3,_("Completed"));
			m_CatMenu->Append(MP_CAT_SET0+4,_("Waiting"));
			m_CatMenu->Append(MP_CAT_SET0+5,_("Downloading"));
			m_CatMenu->Append(MP_CAT_SET0+6,_("Erroneous"));
			m_CatMenu->Append(MP_CAT_SET0+7,_("Paused"));
			m_CatMenu->Append(MP_CAT_SET0+8,_("Stopped"));
			m_CatMenu->AppendSeparator();
			m_CatMenu->Append(MP_CAT_SET0+9,_("Video"));
			m_CatMenu->Append(MP_CAT_SET0+10,_("Audio"));
			m_CatMenu->Append(MP_CAT_SET0+11,_("Archive"));
			m_CatMenu->Append(MP_CAT_SET0+12,_("CD-Images"));
			m_CatMenu->Append(MP_CAT_SET0+13,_("Pictures"));
			m_CatMenu->Append(MP_CAT_SET0+14,_("Text"));
			//m_CatMenu.CheckMenuItem( MP_CAT_SET0+theApp.glob_prefs->GetAllcatType() ,MF_CHECKED | MF_BYCOMMAND);
			menu->Append(47321,_("Select view filter"),m_CatMenu);
		}

		menu->Append(MP_CAT_ADD,_("Add category"));
		menu->Append(MP_CAT_EDIT,_("Edit category"));
		menu->Append(MP_CAT_REMOVE, _("Remove category"));
		menu->AppendSeparator();
		//menu->Append(472834,_("Priority"),m_PrioMenu);

		menu->Append(MP_CANCEL,_("Cancel"));
		menu->Append(MP_STOP, _("&Stop"));
		menu->Append(MP_PAUSE, _("&Pause"));
		menu->Append(MP_RESUME, _("&Resume"));
		//menu->Append(MP_RESUMENEXT, _("Resume next paused"));
		// the point coming from mulenotebook control isn't in screen coordinates
		// (unlike std mouse event, which always returns screen coordinates)
		// so we must do the conversion here
		wxPoint pt=evt.GetPosition();
		wxPoint newpt=nb->ClientToScreen(pt);
		newpt=ScreenToClient(newpt);
		//evt.Skip();
		PopupMenu(menu,newpt);
		delete menu;

		CatMenu=false;
	}
}

bool CTransferWnd::ProcessEvent(wxEvent& evt)
{
	if(evt.GetEventType()!=wxEVT_COMMAND_MENU_SELECTED) {
		return wxPanel::ProcessEvent(evt);
	}
	/*
	if(evt.GetEventObject()!=this)
	return wxPanel::ProcessEvent(evt);
	*/
	wxCommandEvent& event=(wxCommandEvent&)evt;
	switch(event.GetId()) {
		case MP_CAT_SET0: 
		case MP_CAT_SET0+1: 
		case MP_CAT_SET0+2:
		case MP_CAT_SET0+3: 
		case MP_CAT_SET0+4: 
		case MP_CAT_SET0+5: 
		case MP_CAT_SET0+6:
		case MP_CAT_SET0+7:
		case MP_CAT_SET0+8: 
		case MP_CAT_SET0+9:
		case MP_CAT_SET0+10: 
		case MP_CAT_SET0+11:
		case MP_CAT_SET0+12: 
		case MP_CAT_SET0+13: 
		case MP_CAT_SET0+14: {
			theApp.glob_prefs->SetAllcatType(event.GetId()-MP_CAT_SET0);
			theApp.glob_prefs->GetCategory(0)->title = GetCatTitle(theApp.glob_prefs->GetAllcatType());
			EditCatTabLabel(0,theApp.glob_prefs->GetCategory(0)->title);
			downloadlistctrl->ChangeCategory(0);
			downloadlistctrl->InitSort();
			break;
		}

		case MP_CAT_ADD: {
			int newindex=AddCategorie(wxT("?"),char2unicode(theApp.glob_prefs->GetIncomingDir()),wxEmptyString,false);
			//m_dlTab.InsertItem(newindex,theApp.glob_prefs->GetCatego
			//	       ry(newindex)->title);
			wxPanel* nullPanel=new wxPanel(m_dlTab,-1);
			m_dlTab->AddPage(nullPanel,theApp.glob_prefs->GetCategory(newindex)->title);
			CCatDialog dialog(this,newindex);
			dialog.OnInitDialog();
			dialog.ShowModal();

			EditCatTabLabel(newindex,theApp.glob_prefs->GetCategory(newindex)->title);
			theApp.glob_prefs->SaveCats();
			break;
		}
		case MP_CAT_EDIT: {
			CCatDialog dialog(this,m_dlTab->GetSelection());
			dialog.OnInitDialog();
			dialog.ShowModal();

			EditCatTabLabel(m_dlTab->GetSelection(),theApp.glob_prefs->GetCategory(m_dlTab->GetSelection())->title );

			theApp.glob_prefs->SaveCats();
			break;
		}
		case MP_CAT_REMOVE: {
			theApp.downloadqueue->ResetCatParts(m_dlTab->GetSelection());
			theApp.glob_prefs->RemoveCat(m_dlTab->GetSelection());
			m_dlTab->RemovePage(m_dlTab->GetSelection());
			m_dlTab->SetSelection(0);
			downloadlistctrl->ChangeCategory(0);
			theApp.glob_prefs->SaveCats();
			if (theApp.glob_prefs->GetCatCount()==1) {
				theApp.glob_prefs->SetAllcatType(0);
			}
			theApp.amuledlg->searchwnd->UpdateCatChoice();
			break;
		}
		case MP_PRIOLOW: {
			theApp.downloadqueue->SetCatPrio(m_dlTab->GetSelection(),PR_LOW);
			break;
		}
		case MP_PRIONORMAL: {
			theApp.downloadqueue->SetCatPrio(m_dlTab->GetSelection(),PR_NORMAL);
			break;
		}
		case MP_PRIOHIGH: {
			theApp.downloadqueue->SetCatPrio(m_dlTab->GetSelection(),PR_HIGH );
			break;
		}
		case MP_PRIOAUTO: {
			theApp.downloadqueue->SetCatPrio(m_dlTab->GetSelection(),PR_AUTO );
			break;
		}
		case MP_PAUSE: {
			theApp.downloadqueue->SetCatStatus(m_dlTab->GetSelection(),MP_PAUSE);
			break;
		}
		case MP_STOP : {
			theApp.downloadqueue->SetCatStatus(m_dlTab->GetSelection(),MP_STOP);
			break;
		}

		case MP_CANCEL:
			if (wxMessageBox(_("Are you sure you wish to cancel and delete all files in this category?"),_("Confirmation Required"),
			   wxYES_NO|wxCENTRE|wxICON_EXCLAMATION) == wxYES) {
				theApp.downloadqueue->SetCatStatus(m_dlTab->GetSelection(),MP_CANCEL);
			}
			break;

		case MP_RESUME: {
			theApp.downloadqueue->SetCatStatus(m_dlTab->GetSelection(),MP_RESUME);
			break;
		}
		#if 0
		case MP_RESUMENEXT:
			theApp.downloadqueue->StartNextFile(m_dlTab->GetSelection());
			break;
		#endif
		case IDC_UPLOAD_ICO: {
			wxCommandEvent nullEvt;
			SwitchUploadList(nullEvt);
			break;
		}
		case IDC_QUEUE_REFRESH_BUTTON: {
			#warning TODO: fix this
			//OnBnClickedQueueRefreshButton();
			break;
		}
		default: {
			return wxPanel::ProcessEvent(evt);
		}
	}
	return false;
}

int CTransferWnd::AddCategorie(wxString newtitle,wxString newincoming,wxString newcomment,bool addTab){
        Category_Struct* newcat=new Category_Struct;

        newcat->title = newtitle;
        newcat->prio=0;
        newcat->incomingpath = newincoming;
        newcat->comment = newcomment;
        int index=theApp.glob_prefs->AddCat(newcat);

        if (addTab) {
	  //m_dlTab.InsertItem(index,newtitle);
	  wxPanel* nullPanel=new wxPanel(m_dlTab,-1);
	  m_dlTab->AddPage(nullPanel,newtitle);
	}
	theApp.amuledlg->searchwnd->UpdateCatChoice();
        return index;
}

void CTransferWnd::EditCatTabLabel(int index,wxString newlabel)
{
	if (theApp.glob_prefs->ShowCatTabInfos()) {
		CPartFile* cur_file;
		uint16 count,dwl;
		count=dwl=0;
		for (int i=0;i<theApp.downloadqueue->GetFileCount();i++) {
			cur_file=theApp.downloadqueue->GetFileByIndex(i);
			if (cur_file==0) {
				continue;
			}
			if (CheckShowItemInGivenCat(cur_file,index)) {
				count++;
				if (cur_file->GetTransferingSrcCount()>0) {
					dwl++;
				}
			}
		}
		
		newlabel += wxString::Format(wxT(" (%i/%i)"),dwl,count);
	}
	m_dlTab->SetPageText(index,newlabel);
	theApp.amuledlg->searchwnd->UpdateCatChoice();
}

void CTransferWnd::OnSashPositionChanged(wxSplitterEvent& evt)
{
	theApp.amuledlg->split_pos = ((wxSplitterWindow*)FindWindow(wxT("splitterWnd")))->GetSashPosition();
}

void CTransferWnd::OnBtnClearDownloads(wxCommandEvent &evt) {

    downloadlistctrl->Freeze();
    downloadlistctrl->ClearCompleted();
    downloadlistctrl->Thaw();
}

void CTransferWnd::OnBtnSwitchUpload(wxCommandEvent &evt) {
	
	if( windowtransferstate == false) {
		windowtransferstate=true;
		// hide the upload list
		queueSizer->Remove(uploadlistctrl);
		uploadlistctrl->Show(FALSE);
		queueSizer->Add(queuelistctrl,1,wxGROW|wxALIGN_CENTER_VERTICAL ,5);
		queuelistctrl->Show();
		queueSizer->Layout();
	} else {
		windowtransferstate=false;
		// hide the queuelist
		queueSizer->Remove(queuelistctrl);
		queuelistctrl->Show(FALSE);
		queueSizer->Add(uploadlistctrl,1,wxGROW|wxALIGN_CENTER_VERTICAL, 5);
		uploadlistctrl->Show();
		queueSizer->Layout();
	}
}

void CTransferWnd::UpdateCatTabTitles() {
	for (uint8 i=0;i<m_dlTab->GetPageCount();i++) {
		EditCatTabLabel(i,(i==0)? GetCatTitle( theApp.glob_prefs->GetAllcatType() ):theApp.glob_prefs->GetCategory(i)->title);
	}
}
