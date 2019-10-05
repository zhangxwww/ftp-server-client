# client.py

import wx
import wx.lib.masked

import ftp


class Client:
    def __init__(self):

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

        self.app = wx.App()
        self.initGUI()
        self.bindEvents()
        self.app.MainLoop()
        self.ftp = ftp.FTP()

    def connect(self, _):
        print('Connect')

    def login(self, _):
        print('login')

    def quit(self, _):
        print('quit')

    def refresh(self, _):
        print('refresh')

    def upload(self, _):
        print('upload')

    def changdir(self, _):
        print('cd')

    def removedir(self, _):
        print('rm')

    def rename(self, _):
        print('rename')

    def rightClickItem(self, event):
        print('Right click on {}'.format(event.GetIndex()))

    def initGUI(self):
        self.main_window = wx.Frame(None, title='FTP Client', size=(640, 976))
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
        self.file_list.Bind(wx.EVT_LIST_ITEM_RIGHT_CLICK, self.rightClickItem)

    def initConnectionArea(self, panel, vbox):
        connection_info = wx.StaticBox(panel, -1, 'Connection info')

        connection_sizer = wx.StaticBoxSizer(connection_info, wx.VERTICAL)
        connection_box = wx.BoxSizer(wx.HORIZONTAL)

        ip_text = wx.StaticText(panel, -1, 'IP')
        port_text = wx.StaticText(panel, -1, 'Port')
        ip_input = wx.lib.masked.IpAddrCtrl(panel, -1)
        port_input = wx.TextCtrl(panel, -1, style=wx.ALIGN_LEFT)
        connect_button = wx.Button(panel, -1, 'Connect')

        connection_box.Add(ip_text, 0, wx.ALL | wx.CENTER, 5)
        connection_box.Add(ip_input, 0, wx.ALL | wx.CENTER, 10)
        connection_box.Add(port_text, 0, wx.ALL | wx.CENTER, 5)
        connection_box.Add(port_input, 0, wx.ALL | wx.CENTER, 5)
        connection_box.Add(connect_button, 0, wx.ALL | wx.CENTER, 5)
        connection_sizer.Add(connection_box, 0, wx.ALL | wx.CENTER, 10)

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
        connection_sizer.Add(login_box, 0, wx.ALL | wx.CENTER, 10)

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

        prompt_show = wx.TextCtrl(panel, -1, style=wx.ALIGN_LEFT | wx.TE_MULTILINE | wx.TE_READONLY, size=(-1, 240))
        prompt_input = wx.TextCtrl(panel, -1, style=wx.ALIGN_LEFT)

        prompt_sizer.Add(prompt_show, 0, wx.TOP | wx.LEFT | wx.RIGHT | wx.CENTER | wx.EXPAND, 10)
        prompt_sizer.Add(prompt_input, 0, wx.BOTTOM | wx.LEFT | wx.RIGHT | wx.CENTER | wx.EXPAND, 10)

        vbox.Add(prompt_sizer, 0, wx.ALL | wx.CENTER | wx.EXPAND, 5)

        self.prompt_show = prompt_show
        self.prompt_input = prompt_input

    def initExplorerArea(self, panel, vbox):
        explorer = wx.StaticBox(panel, -1, 'Explorer')

        explorer_sizer = wx.StaticBoxSizer(explorer, wx.VERTICAL)
        higher_box = wx.BoxSizer(wx.HORIZONTAL)

        cwd = wx.StaticText(panel, -1, 'Unconnected', style=(wx.TE_LEFT | wx.ALIGN_LEFT | wx.EXPAND))

        higher_box.Add(cwd, 0, wx.ALL | wx.ALIGN_LEFT | wx.EXPAND, 5)

        explorer_sizer.Add(higher_box, 0, wx.ALL | wx.CENTER, 10)

        file_list = wx.ListCtrl(panel, -1, style=wx.LC_REPORT, size=(-1, 240))
        file_list.AppendColumn('Type')
        file_list.AppendColumn('Filename')
        file_list.AppendColumn('Size')
        file_list.AppendColumn('Last modified')

        file_list.Append(["1", "2", "3", "4"])
        explorer_sizer.Add(file_list, 0, wx.ALL | wx.CENTER | wx.EXPAND, 10)

        lower_box = wx.BoxSizer(wx.HORIZONTAL)
        upload = wx.Button(panel, -1, 'Upload')
        refresh = wx.Button(panel, -1, 'Refresh')
        pasv = wx.CheckBox(panel, -1, 'Pasv')

        lower_box.Add(refresh, 0, wx.ALL | wx.CENTER, 5)
        lower_box.Add(upload, 0, wx.ALL | wx.CENTER, 5)
        lower_box.Add(pasv, 0, wx.ALL | wx.CENTER, 5)

        explorer_sizer.Add(lower_box, 0, wx.ALL | wx.CENTER, 10)

        vbox.Add(explorer_sizer, 0, wx.ALL | wx.CENTER | wx.EXPAND, 5)

        self.cwd = cwd
        self.file_list = file_list
        self.upload_button = upload
        self.refresh_button = refresh
        self.pasv_box = pasv

    @staticmethod
    def initBottom(panel, vbox):
        info = wx.StaticText(panel, -1, 'Copyright © Zhang Xinwei 2019')
        vbox.Add(info, 0, wx.ALL | wx.CENTER | wx.EXPAND, 5)


if __name__ == '__main__':
    Client()
