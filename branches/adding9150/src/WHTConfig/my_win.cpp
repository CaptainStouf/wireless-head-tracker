#include "stdafx.h"

#include "my_utils.h"
#include "my_win.h"

Icon::Icon(int iconID, bool is_large)
	: _is_large(is_large)
{
	const int pixels = _is_large ? 48 : 16;
	_hIcon = (HICON) ::LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(iconID), IMAGE_ICON, pixels, pixels, LR_SHARED);
}

std::wstring Window::GetText()
{
	const int BUFF_SIZE = 2048;		// should be enough, i guess...
	wchar_t buff[BUFF_SIZE];

	int bytes_copied = ::GetWindowText(_hWnd, buff, BUFF_SIZE);

	return buff;
}

LRESULT CALLBACK Dialog::DialogProcedure(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	Dialog* pDlg = (Dialog*)(::GetWindowLong(hwnd, GWL_USERDATA));

	try {
		if (message == WM_INITDIALOG)
		{
			// get the dialog<> pointer as the creation param
			pDlg = reinterpret_cast<Dialog*> (lParam);
			pDlg->_hWnd = hwnd;

			// set the dialog pointer
			::SetWindowLong(hwnd, GWL_USERDATA, (LONG) pDlg);

			pDlg->OnInit();		// send the init dialog message

			return TRUE;

		} else if (pDlg == 0) {
			return FALSE;
		}

		switch (message)
		{
		case WM_CLOSE:
			pDlg->OnClose();
			return TRUE;
		case WM_DESTROY:
			pDlg->OnDestroy();

			// clear the dialog pointer
			::SetWindowLong(hwnd, GWL_USERDATA, 0);

			return TRUE;
		case WM_MOUSEMOVE:
			pDlg->OnMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), wParam);
			break;
		case WM_LBUTTONDOWN:
			pDlg->OnLButtonDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), wParam);
			break;
		case WM_LBUTTONUP:
			pDlg->OnLButtonUp(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), wParam);
			break;
		case WM_SIZE:
			pDlg->OnSize(LOWORD(lParam), HIWORD(lParam), wParam);
			break;
		case WM_TIMER:
			pDlg->OnTimer(wParam);
			break;
		case WM_MOUSEWHEEL:
			pDlg->OnMouseWheel(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), GET_WHEEL_DELTA_WPARAM(wParam), wParam);
			return TRUE;
		case WM_COMMAND:

			if (HIWORD(wParam) == 0  &&  lParam == 0)
				pDlg->OnMenu(LOWORD(lParam));
			else if (HIWORD(wParam) == 1  &&  lParam == 0)
				pDlg->OnAccelerator(LOWORD(lParam));
			else
				pDlg->OnControl(LOWORD(wParam), HIWORD(wParam), (HWND) lParam);

			return TRUE;

		case WM_SYSCOMMAND:
			if (pDlg->OnSysCommand(wParam & 0xfff0))
				return TRUE;
			break;
		case WM_TRAYNOTIFY:
			pDlg->OnTrayNotify(lParam);
			return TRUE;
		}

	} catch (std::wstring& e) {
		if (pDlg != 0)
			pDlg->OnException(e);
		else
			::MessageBox(hwnd, e.c_str(), L"Exception", MB_OK | MB_ICONERROR);
	}

	return FALSE;
}

void Dialog::SetIcon(const Icon& ic)
{
	SendMessage(_hWnd, WM_SETICON, ic.IsLarge() ? ICON_BIG : ICON_SMALL, (LPARAM) ic.GetHandle());
} 

bool Dialog::CreateDlg(int dlgID, HWND hWndParent)
{
	HWND hDlg = ::CreateDialogParam(::GetModuleHandle(NULL),
									MAKEINTRESOURCE(dlgID),
									hWndParent,
									(DLGPROC) DialogProcedure,
									(LPARAM) this);

	return hDlg != NULL;
}