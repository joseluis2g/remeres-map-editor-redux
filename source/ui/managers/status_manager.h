//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_STATUS_MANAGER_H_
#define RME_STATUS_MANAGER_H_

#include "app/main.h"

#include <chrono>

class StatusManager {
public:
	void SetTitle(const wxString& title);
	void UpdateTitle();
	void SetStatusText(const wxString& text, int field = 0);

	/**
	 * Writes an operation summary (file, size, elapsed) to the result field and
	 * keeps it there for `hold`, so the periodic FPS tick cannot erase it before
	 * the user has read it. Any explicit SetStatusText() on the same field takes
	 * over immediately.
	 */
	void SetResultText(const wxString& text, std::chrono::seconds hold = std::chrono::seconds(6));

	/**
	 * Writes the FPS counter only when no result summary is being held; called
	 * by the render loop, which would otherwise overwrite field 0 every second.
	 */
	void SetFpsText(const wxString& text);

private:
	static constexpr int kResultField = 0;

	bool ResultHeld() const;
	void WriteStatusText(const wxString& text, int field);

	std::chrono::steady_clock::time_point result_until;
};

extern StatusManager g_status;

#endif
