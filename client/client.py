# client.py

import wx
import re
import os
import _thread

import ftp


class Client:
    def __init__(self):

        # ??????
        self.main_window = None
        self.ip_input = None
        self.port_input = None
        self.connect_button = None
        self.username_input = None
        self.password_input = None
        self.login_button = None
        self.quit_button = None
        self.prompt_show = None
        self.prompt_input = None
        self.cwd = None
        self.file_list = None
        self.upload_button = None
        self.refresh_button = None
        self.pasv_box = None
        self.cd_button = None
        self.rm_button = None
        self.rename_button = None
        self.download_button = None
        self.mkd_button = None

        self.files = []
        self.selected_row = None
        self.connected = False
        self.trans_lock = False

        self.ftp = ftp.FTP()
        self.app = wx.App()
        self.initGUI()
        self.bindEvents()
        self.app.MainLoop()

    def connect(self, _):
        host = self.ip_input.GetValue()
        port = self.port_input.GetValue()
        try:
            port = int(port)
            if port > 65535:
                self.showPrompt((None, 'Invalid port'))
                return
        except ValueError:
            self.showPrompt((None, 'Invalid port'))
            return
        res = self.ftp.connect(host, port)
        self.showPrompt(res)
        self.connected = True

    def login(self, _):
        if not self.connected:
            self.showPrompt((None, "Please connect to server"))
            return
        username = self.username_input.GetValue()
        password = self.password_input.GetValue()
        cmd, res = self.ftp.USER(username)
        self.showPrompt((cmd, res))
        if res[0] != '3':
            return
        cmd, res = self.ftp.PASS(password)
        self.showPrompt((cmd, res))
        if res[0] != '2':
            return
        self.connected = True
        cmd, res = self.ftp.SYST()
        self.showPrompt((cmd, res))
        self.refresh(None)

    def quit(self, _):
        if not self.connected:
            return
        res = self.ftp.QUIT()
        self.showPrompt(res)
        self.connected = False
        self.file_list.ClearAll()
        self.file_list.AppendColumn('Type')
        self.file_list.AppendColumn('Filename')
        self.file_list.AppendColumn('Size')
        self.file_list.AppendColumn('Last modified')
        self.resizeList()
        self.cwd.SetLabelText('Unconnected')
        self.cd_button.Disable()
        self.rm_button.Disable()
        self.download_button.Disable()
        self.rename_button.Disable()

    def refresh(self, _):
        self.updatePWD()
        self.updateList()

    def upload(self, _):
        file_dialog = wx.FileDialog(self.main_window, message='Choose upload file', style=wx.FD_OPEN)
        dia_res = file_dialog.ShowModal()
        if dia_res != wx.ID_OK:
            return
        path = file_dialog.GetPath()
        filename = path.split(os.sep)[-1]
        cmd, res = self.ftp.TYPE('I')
        self.showPrompt((cmd, res))
        if res[0] != '2':
            return
        if self.pasv_box.GetValue():
            cmd, res = self.ftp.PASV()
        else:
            cmd, res = self.ftp.PORT()
        self.showPrompt((cmd, res))
        if res[0] != '2':
            return

        @self.requireLock()
        def thread_func():
            total_byte = os.path.getsize(path)
            dialog = wx.ProgressDialog('Uploading...', '')

            def getCallback(total, dia):
                def callback(n_byte):
                    percent = 100 * n_byte // total
                    self.main_window.SetTitle('FTP Client Uploading... {}% finished'.format(percent))
                    dia.Update(percent, '{}%'.format(percent))
                return callback

            cmd_, res_ = self.ftp.STOR(filename, open(path, 'rb'), getCallback(total_byte, dialog))

            if isinstance(res_, str):
                self.showPrompt((cmd_, res_))
            elif isinstance(res_, tuple):
                self.showPrompt((cmd_, res_[0]))
                self.showPrompt((None, res_[1]))
            self.main_window.SetTitle('FTP Client')
            if res_[0] != '2':
                return
            self.updateList()

        _thread.start_new_thread(thread_func, ())

    def changeDir(self, _):
        dir_name = self.selected_row[1]
        if dir_name == '..':
            cmd, res = self.ftp.CDUP()
        else:
            cmd, res = self.ftp.CWD(dir_name)
        self.showPrompt((cmd, res))
        if res[0] != '2':
            return
        self.refresh(None)

    def removeDir(self, _):
        dir_name = self.selected_row[1]
        cmd, res = self.ftp.RMD(dir_name)
        self.showPrompt((cmd, res))
        if res[0] != '2':
            return
        self.refresh(None)

    def makeDir(self, _):
        make_dialog = wx.TextEntryDialog(None, '', 'Directory name', '')
        if make_dialog.ShowModal() != wx.ID_OK:
            return
        dir_name = make_dialog.GetValue()
        cmd, res = self.ftp.MKD(dir_name)
        self.showPrompt((cmd, res))
        if res[0] != '2':
            return
        self.refresh(None)

    def download(self, _):
        save_dialog = wx.FileDialog(self.main_window,
                                    message='Choose upload file',
                                    style=wx.FD_SAVE |wx.FD_OVERWRITE_PROMPT)
        save_res = save_dialog.ShowModal()
        if save_res != wx.ID_OK:
            return
        path = save_dialog.GetPath()
        filename = self.selected_row[1]
        cmd, res = self.ftp.TYPE('I')
        self.showPrompt((cmd, res))
        if res[0] != '2':
            return
        if self.pasv_box.GetValue():
            cmd, res = self.ftp.PASV()
        else:
            cmd, res = self.ftp.PORT()
        self.showPrompt((cmd, res))
        if res[0] != '2':
            return

        @self.requireLock()
        def thread_func():
            dialog = wx.ProgressDialog('Downloading...', '')

            def getCallback(dia):
                def callback(total, cur):
                    if total is not None:
                        percent = 100 * cur // total
                        self.main_window.SetTitle('FTP Client Downloading... {}% finished'.format(percent))
                        dia.Update(percent, '{}%'.format(percent))
                    else:
                        self.main_window.SetTitle('FTP Client Downloading...')
                        dia.Update(0, '{} finished'.format(self.readable_size(cur)))
                return callback

            cmd_, res_ = self.ftp.RETR(filename, open(path, 'wb'), getCallback(dialog))
            dialog.Update(100)
            if isinstance(res_, str):
                self.showPrompt((cmd_, res_))
            elif isinstance(res_, tuple):
                self.showPrompt((cmd_, res_[0]))
                self.showPrompt((None, res_[1]))
            self.main_window.SetTitle('FTP Client')

        _thread.start_new_thread(thread_func, ())

    def rename(self, _):
        old = self.selected_row[1]
        rename_dialog = wx.TextEntryDialog(None, '', 'Please enter the new name', '')
        if rename_dialog.ShowModal() != wx.ID_OK:
            return
        new = rename_dialog.GetValue()
        cmd, res = self.ftp.RNFR(old)
        self.showPrompt((cmd, res))
        if res[0] != '3':
            self.showPrompt((cmd, 'Invalid response, fail to rename'))
        cmd, res = self.ftp.RNTO(new)
        self.showPrompt((cmd, res))
        if res[0] != '2':
            return
        self.refresh(None)

    # event handler when left click at list items
    def leftClickItem(self, event):
        if self.trans_lock:
            return
        row = self.files[event.GetIndex()]
        if row[0] == 'd':
            self.cd_button.Enable()
            self.rm_button.Enable()
            self.rename_button.Disable()
            self.download_button.Disable()
        else:
            self.rename_button.Enable()
            self.download_button.Enable()
            self.cd_button.Disable()
            self.rm_button.Disable()
        self.selected_row = row

    def execUserCmd(self, _):
        cmd = self.prompt_input.GetValue()
        self.prompt_input.Clear()
        try:
            res = self.ftp.send_command(cmd)
        except IOError as e:
            res = str(e)
        self.showPrompt((cmd, res))

    # show cmd and response on the prompt
    def showPrompt(self, line):
        prompt = ''
        cmd, res = line
        if cmd:
            prompt = 'ftp> ' + prompt + cmd + '\n'
        if isinstance(res, tuple) or isinstance(res, list):
            prompt = prompt + ''.join(res)
        if isinstance(res, str):
            prompt = prompt + res
        if not prompt[-1] == '\n':
            prompt = prompt + '\n'
        self.prompt_show.write(prompt)

    def updatePWD(self):
        cmd, res = self.ftp.PWD()
        reg = re.compile(r'["\'](.*?)["\']')
        match = reg.search(res)
        if match is None:
            return
        wd = match.groups()[0]
        self.cwd.SetLabelText(wd)
        self.showPrompt((cmd, res))

    def updateList(self):
        if self.pasv_box.GetValue():
            cmd, res = self.ftp.PASV()
        else:
            cmd, res = self.ftp.PORT()
        self.showPrompt((cmd, res))
        if res[0] != '2':
            return
        cmd, res = self.ftp.TYPE('A')
        self.showPrompt((cmd, res))

        @self.requireLock()
        def thread_func():
            cmd_, res_, lines = self.ftp.LIST(None)
            self.showPrompt((cmd_, res_[0]))
            self.showPrompt((None, res_[1]))
            if res_[1][0] != '2':
                return
            parsed = []
            for line in lines:
                p = self.parseListEachLine(line)
                if p:
                    parsed.append(p)
            parsed = sorted(parsed, key=lambda x: x[1])
            if len(parsed) > 0 and parsed[0][1][:1] != '.':
                parsed.insert(0, ('d', '.', '', ''))
            if len(parsed) > 1 and parsed[1][1][:2] != '..':
                parsed.insert(1, ('d', '..', '', ''))
            self.file_list.ClearAll()
            self.file_list.AppendColumn('Type')
            self.file_list.AppendColumn('Filename')
            self.file_list.AppendColumn('Size')
            self.file_list.AppendColumn('Last modified')
            for p in parsed:
                self.file_list.Append(p)
            self.files = parsed
            self.resizeList()

        _thread.start_new_thread(thread_func, ())

    def disableButtons(self):
        for b in [self.connect_button,
                  self.login_button,
                  self.quit_button,
                  self.upload_button,
                  self.refresh_button,
                  self.cd_button,
                  self.rm_button,
                  self.rename_button,
                  self.download_button,
                  self.mkd_button]:
            b.Disable()

    def enableButtons(self):
        for b in [self.connect_button,
                  self.login_button,
                  self.quit_button,
                  self.refresh_button,
                  self.upload_button,
                  self.mkd_button]:
            b.Enable()

    def requireLock(self):
        def requireLockDec(f):
            def g(*args, **kwargs):
                self.disableButtons()
                if self.trans_lock:
                    return
                self.trans_lock = True
                f(*args, **kwargs)
                self.trans_lock = False
                self.enableButtons()
            return g
        return requireLockDec

    # extract file info from LIST command
    def parseListEachLine(self, line):
        line = line.strip()
        reg = re.compile(r'^([slbdpc-])[wrx-]{9}\s*\d*\s*\S*\s*\S*\s*(\d*)\s*([A-Za-z]*\s*\d*\s*\d*(:\d*)?)\s*(.*)$')
        match = reg.search(line)
        if not match:
            return
        file_type, file_size, last_modified, _, filename = match.groups()
        file_size = self.readable_size(int(file_size))
        return file_type, filename, file_size, last_modified

    # generate readable size of files
    @staticmethod
    def readable_size(file_size):
        suffixes = ['B', 'KB', 'MB']
        suf = 'GB'
        for s in suffixes:
            if file_size > 1024:
                file_size = file_size // 1024
            else:
                suf = s
                break
        return str(file_size) + ' ' + suf

    ########################### The rest is ALL about GUI ############################

    def initGUI(self):
        self.main_window = wx.Frame(None, title='FTP Client', size=(640, 768))
        panel = wx.Panel(self.main_window)

        vbox = wx.BoxSizer(wx.VERTICAL)
        self.initConnectionArea(panel, vbox)
        self.initPromptArea(panel, vbox)
        self.initExplorerArea(panel, vbox)
        self.initBottom(panel, vbox)
        panel.SetSizer(vbox)
        panel.Fit()

        self.main_window.Show(True)

    def bindEvents(self):
        self.connect_button.Bind(wx.EVT_BUTTON, self.connect)
        self.login_button.Bind(wx.EVT_BUTTON, self.login)
        self.quit_button.Bind(wx.EVT_BUTTON, self.quit)
        self.refresh_button.Bind(wx.EVT_BUTTON, self.refresh)
        self.upload_button.Bind(wx.EVT_BUTTON, self.upload)
        self.cd_button.Bind(wx.EVT_BUTTON, self.changeDir)
        self.rm_button.Bind(wx.EVT_BUTTON, self.removeDir)
        self.download_button.Bind(wx.EVT_BUTTON, self.download)
        self.rename_button.Bind(wx.EVT_BUTTON, self.rename)
        self.mkd_button.Bind(wx.EVT_BUTTON, self.makeDir)
        self.file_list.Bind(wx.EVT_LIST_ITEM_SELECTED, self.leftClickItem)
        self.main_window.Bind(wx.EVT_TEXT_ENTER, self.execUserCmd, self.prompt_input)

    def initConnectionArea(self, panel, vbox):
        connection_info = wx.StaticBox(panel, -1, 'Connection info')

        connection_sizer = wx.StaticBoxSizer(connection_info, wx.VERTICAL)
        connection_box = wx.BoxSizer(wx.HORIZONTAL)

        ip_text = wx.StaticText(panel, -1, 'Host')
        port_text = wx.StaticText(panel, -1, 'Port')
        ip_input = wx.TextCtrl(panel, -1)
        # ip_input.SetValue('ftp.sjtu.edu.cn')
        ip_input.SetValue('127.0.0.1')
        port_input = wx.TextCtrl(panel, -1, style=wx.ALIGN_LEFT, validator=PortValidator())
        port_input.SetValue('21')
        connect_button = wx.Button(panel, -1, 'Connect')

        connection_box.Add(ip_text, 0, wx.ALL | wx.CENTER, 5)
        connection_box.Add(ip_input, 0, wx.ALL | wx.CENTER, 5)
        connection_box.Add(port_text, 0, wx.ALL | wx.CENTER, 5)
        connection_box.Add(port_input, 0, wx.ALL | wx.CENTER, 5)
        connection_box.Add(connect_button, 0, wx.ALL | wx.CENTER, 5)
        connection_sizer.Add(connection_box, 0, wx.ALL | wx.CENTER, 5)

        login_box = wx.BoxSizer(wx.HORIZONTAL)
        username_text = wx.StaticText(panel, -1, 'Username')
        password_text = wx.StaticText(panel, -1, 'Password')
        username_input = wx.TextCtrl(panel, -1, style=wx.ALIGN_LEFT)
        password_input = wx.TextCtrl(panel, -1, style=wx.ALIGN_LEFT | wx.TE_PASSWORD)
        login_button = wx.Button(panel, -1, 'Login')
        quit_button = wx.Button(panel, -1, 'Quit')
        login_box.Add(username_text, 0, wx.ALL | wx.CENTER, 5)
        login_box.Add(username_input, 0, wx.ALL | wx.CENTER, 5)
        login_box.Add(password_text, 0, wx.ALL | wx.CENTER, 5)
        login_box.Add(password_input, 0, wx.ALL | wx.CENTER, 5)
        login_box.Add(login_button, 0, wx.ALL | wx.CENTER, 5)
        login_box.Add(quit_button, 0, wx.ALL | wx.CENTER, 5)
        connection_sizer.Add(login_box, 0, wx.ALL | wx.CENTER, 5)

        username_input.SetValue('anonymous')
        password_input.SetValue('anonymous@')

        vbox.Add(connection_sizer, 0, wx.ALL | wx.CENTER | wx.EXPAND, 5)

        self.ip_input = ip_input
        self.port_input = port_input
        self.connect_button = connect_button
        self.username_input = username_input
        self.password_input = password_input
        self.login_button = login_button
        self.quit_button = quit_button

    def initPromptArea(self, panel, vbox):
        prompt = wx.StaticBox(panel, -1, 'Prompt')

        prompt_sizer = wx.StaticBoxSizer(prompt, wx.VERTICAL)

        prompt_show = wx.TextCtrl(panel, -1, style=wx.ALIGN_LEFT | wx.TE_MULTILINE | wx.TE_READONLY, size=(-1, 150))
        prompt_input = wx.TextCtrl(panel, -1, style=wx.ALIGN_LEFT | wx.TE_PROCESS_ENTER)

        prompt_sizer.Add(prompt_show, 0, wx.TOP | wx.LEFT | wx.RIGHT | wx.CENTER | wx.EXPAND, 5)
        prompt_sizer.Add(prompt_input, 0, wx.BOTTOM | wx.LEFT | wx.RIGHT | wx.CENTER | wx.EXPAND, 5)

        vbox.Add(prompt_sizer, 0, wx.ALL | wx.CENTER | wx.EXPAND, 5)

        self.prompt_show = prompt_show
        self.prompt_input = prompt_input

    def initExplorerArea(self, panel, vbox):
        explorer = wx.StaticBox(panel, -1, 'Explorer')

        explorer_sizer = wx.StaticBoxSizer(explorer, wx.VERTICAL)
        higher_box = wx.BoxSizer(wx.HORIZONTAL)

        cwd = wx.StaticText(panel, -1, 'Unconnected', style=(wx.TE_LEFT | wx.ALIGN_LEFT | wx.EXPAND))

        higher_box.Add(cwd, 0, wx.ALL | wx.ALIGN_LEFT | wx.EXPAND, 5)

        explorer_sizer.Add(higher_box, 0, wx.ALL | wx.CENTER, 5)

        file_list = wx.ListCtrl(panel, -1, style=wx.LC_REPORT, size=(-1, 150))
        file_list.AppendColumn('Type')
        file_list.AppendColumn('Filename')
        file_list.AppendColumn('Size')
        file_list.AppendColumn('Last modified')

        explorer_sizer.Add(file_list, 0, wx.ALL | wx.CENTER | wx.EXPAND, 5)

        lower_box = wx.BoxSizer(wx.HORIZONTAL)
        lower_lower_box = wx.BoxSizer(wx.HORIZONTAL)
        upload = wx.Button(panel, -1, 'Upload')
        download = wx.Button(panel, -1, 'Download')
        mkd = wx.Button(panel, -1, 'Make Directory')
        rename = wx.Button(panel, -1, 'Rename')
        cd = wx.Button(panel, -1, 'Change Directory')
        rm = wx.Button(panel, -1, 'Remove Directory')
        refresh = wx.Button(panel, -1, 'Refresh')
        pasv = wx.CheckBox(panel, -1, 'Pasv')
        pasv.SetValue(True)
        download.Disable()
        rename.Disable()
        cd.Disable()
        rm.Disable()

        lower_box.Add(refresh, 0, wx.ALL | wx.CENTER, 5)
        lower_box.Add(upload, 0, wx.ALL | wx.CENTER, 5)
        lower_box.Add(mkd, 0, wx.ALL | wx.CENTER, 5)
        lower_box.Add(download, 0, wx.ALL | wx.CENTER, 5)
        lower_lower_box.Add(rename, 0, wx.ALL | wx.CENTER, 5)
        lower_lower_box.Add(cd, 0, wx.ALL | wx.CENTER, 5)
        lower_lower_box.Add(rm, 0, wx.ALL | wx.CENTER, 5)
        lower_lower_box.Add(pasv, 0, wx.ALL | wx.CENTER, 5)

        explorer_sizer.Add(lower_box, 0, wx.ALL | wx.CENTER, 5)
        explorer_sizer.Add(lower_lower_box, 0, wx.ALL | wx.CENTER, 5)

        vbox.Add(explorer_sizer, 0, wx.ALL | wx.CENTER | wx.EXPAND, 5)

        self.cwd = cwd
        self.file_list = file_list
        self.upload_button = upload
        self.download_button = download
        self.rename_button = rename
        self.cd_button = cd
        self.rm_button = rm
        self.refresh_button = refresh
        self.pasv_box = pasv
        self.mkd_button = mkd

        self.resizeList()

    @staticmethod
    def initBottom(panel, vbox):
        info = wx.StaticText(panel, -1, 'Copyright © Zhang Xinwei 2019')
        vbox.Add(info, 0, wx.ALL | wx.CENTER | wx.EXPAND, 5)

    def resizeList(self):
        self.file_list.SetColumnWidth(0, 40)
        self.file_list.SetColumnWidth(1, 250)
        self.file_list.SetColumnWidth(2, 150)
        self.file_list.SetColumnWidth(3, 150)

# ??????????
class PortValidator(wx.Validator):
    def __init__(self):
        wx.Validator.__init__(self)
        self.valid_char = list(map(str, range(10)))
        self.len = 0
        self.Bind(wx.EVT_CHAR, self.OnCharChanged)

    def Clone(self):
        return PortValidator()

    def Validate(self, win):
        return True

    def TransferToWindow(self):
        return True

    def OnCharChanged(self, event):
        key = event.GetKeyCode()
        if key == 8:
            self.len -= 1
            if self.len < 0:
                self.len = 0
            event.Skip()
            return
        char = chr(key)
        if char in self.valid_char:
            if self.len < 5:
                self.len += 1
                event.Skip()
                return True
        return False


if __name__ == '__main__':
    Client()
