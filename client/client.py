# client.py

import wx
import wx.lib.masked

class Client:
    def __init__(self):
        self.app = wx.App()
        self.initGUI()
        self.app.MainLoop()

    def initGUI(self):
        main_window = wx.Frame(None, title='FTP Client', size=(640, 976))
        panel = wx.Panel(main_window)

        vbox = wx.BoxSizer(wx.VERTICAL)
        self.initConnectionArea(panel, vbox)
        self.initPromptArea(panel, vbox)
        self.initExplorerArea(panel, vbox)
        self.initBottom(panel, vbox)
        panel.SetSizer(vbox)
        panel.Fit()

        main_window.Show(True)

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

    def initPromptArea(self, panel, vbox):
        prompt = wx.StaticBox(panel, -1, 'Prompt')

        prompt_sizer = wx.StaticBoxSizer(prompt, wx.VERTICAL)

        prompt_show = wx.TextCtrl(panel, -1, style=wx.ALIGN_LEFT | wx.TE_MULTILINE | wx.TE_READONLY, size=(-1, 240))
        prompt_input = wx.TextCtrl(panel, -1, style=wx.ALIGN_LEFT)

        prompt_sizer.Add(prompt_show, 0, wx.TOP | wx.LEFT | wx.RIGHT | wx.CENTER | wx.EXPAND, 10)
        prompt_sizer.Add(prompt_input, 0, wx.BOTTOM | wx.LEFT | wx.RIGHT | wx.CENTER | wx.EXPAND, 10)

        vbox.Add(prompt_sizer, 0, wx.ALL | wx.CENTER | wx.EXPAND, 5)

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

    def initBottom(self, panel, vbox):
        info = wx.StaticText(panel, -1, 'Copyright © Zhang Xinwei 2019')
        vbox.Add(info, 0, wx.ALL | wx.CENTER | wx.EXPAND, 5)

if __name__ == '__main__':
    Client()
