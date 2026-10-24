//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/main.h"
#include "ui/managers/status_manager.h"
#include "ui/gui.h"
#include "editor/editor_tabs.h"

StatusManager g_status;

void StatusManager::WriteStatusText(const wxString& text, int field) {
	if (g_gui.root) {
		g_gui.root->SetStatusText(text, field);
	}
}

void StatusManager::SetStatusText(const wxString& text, int field) {
	if (field == kResultField) {
		// A normal message takes the field over; it must not inherit the hold of
		// a previous summary.
		result_until = {};
	}
	WriteStatusText(text, field);
}

bool StatusManager::ResultHeld() const {
	return std::chrono::steady_clock::now() < result_until;
}

void StatusManager::SetResultText(const wxString& text, std::chrono::seconds hold) {
	result_until = std::chrono::steady_clock::now() + hold;
	WriteStatusText(text, kResultField);
}

void StatusManager::SetFpsText(const wxString& text) {
	if (ResultHeld()) {
		return;
	}
	WriteStatusText(text, kResultField);
}

void StatusManager::SetTitle(const wxString& title) {
	if (g_gui.root == nullptr) {
		return;
	}

#ifdef NIGHTLY_BUILD
	#ifdef SVN_BUILD
		#define TITLE_APPEND (wxString(" (Nightly Build #") << i2ws(SVN_BUILD) << ")")
	#else
		#define TITLE_APPEND (wxString(" (Nightly Build)"))
	#endif
#else
	#ifdef SVN_BUILD
		#define TITLE_APPEND (wxString(" (Build #") << i2ws(SVN_BUILD) << ")")
	#else
		#define TITLE_APPEND (wxString(""))
	#endif
#endif

#ifdef __EXPERIMENTAL__
	if (!title.empty()) {
		g_gui.root->SetTitle(title + " - OTAcademy Map Editor BETA" + TITLE_APPEND);
	} else {
		g_gui.root->SetTitle(wxString("OTAcademy Map Editor BETA") + TITLE_APPEND);
	}
#elif __SNAPSHOT__
	if (!title.empty()) {
		g_gui.root->SetTitle(title + " - OTAcademy Map Editor - SNAPSHOT" + TITLE_APPEND);
	} else {
		g_gui.root->SetTitle(wxString("OTAcademy Map Editor - SNAPSHOT") + TITLE_APPEND);
	}
#else
	if (!title.empty()) {
		g_gui.root->SetTitle(title + " - OTAcademy Map Editor" + TITLE_APPEND);
	} else {
		g_gui.root->SetTitle(wxString("OTAcademy Map Editor") + TITLE_APPEND);
	}
#endif
}

void StatusManager::UpdateTitle() {
	if (g_gui.tabbook && g_gui.tabbook->GetTabCount() > 0) {
		SetTitle(g_gui.tabbook->GetCurrentTab()->GetTitle());
		for (int idx = 0; idx < g_gui.tabbook->GetTabCount(); ++idx) {
			EditorTab* tab = g_gui.tabbook->GetTab(idx);
			if (tab) {
				g_gui.tabbook->SetTabLabel(idx, tab->GetTitle());
			}
		}
	} else {
		SetTitle("");
	}
}
