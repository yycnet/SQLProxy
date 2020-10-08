Public Class frmMain

    Public bPrivateUserName As Boolean = False  '是否允许集群数据库采用不同的帐号密码访问
    Public bSQLSubscribe As Boolean = False     '是否允许启用SQL发布订阅机制进行数据同步
    Public bRollBack As Boolean = False          '同步失败是否所有服务器数据回滚
    Private g_dbuser As String, g_dbpswd As String 'bPrivateUserName=false有效，可设置所有数据库公共访问帐号密码(也可不设置)
    Private iPort_Proxy As Integer = 1433
    Private bStartProxy As Boolean = False 'SQLPRoxy是否已经启动
    Private Structure PanelServer
        Dim pan As Panel
        Dim picbox As PictureBox
        Dim info As TextBox
        Dim picdel As PictureBox
        Dim picedit As PictureBox
    End Structure

    Private Function GetPanelServerByIndex(ByVal idx As Integer, ByRef pansvr As PanelServer) As Boolean
        Dim retControl As Control = Nothing
        idx = idx + 1
        retControl = FindControlByName(tpgdbs, "panSever" & idx)
        If retControl Is Nothing Then Return False
        pansvr.pan = CType(retControl, Panel)

        retControl = FindControlByName(tpgdbs, "PictureBox" & idx)
        pansvr.picbox = CType(retControl, PictureBox)
        retControl = FindControlByName(tpgdbs, "serverinfo" & idx)
        pansvr.info = CType(retControl, TextBox)
        retControl = FindControlByName(tpgdbs, "picDel" & idx)
        pansvr.picdel = CType(retControl, PictureBox)
        retControl = FindControlByName(tpgdbs, "picEdit" & idx)
        pansvr.picedit = CType(retControl, PictureBox)
        Return True
    End Function

    Private Sub DisabledModify(ByVal idx As Integer, ByVal bHidden As Boolean)
        Dim pansvr As PanelServer
        If GetPanelServerByIndex(idx, pansvr) Then
            pansvr.picdel.Visible = False
            pansvr.picedit.Visible = False
            RemoveHandler pansvr.picdel.Click, AddressOf picDel_Click
            RemoveHandler pansvr.picedit.Click, AddressOf picEdit_Click

            If bHidden Then pansvr.pan.Visible = False
        End If
    End Sub

    Private Sub SetDBServerInfo(ByVal idx As Integer, ByVal clients As Integer, ByVal querys As Integer, ByVal querys_write As Integer, ByVal dbconnnums As Integer)
        Dim pansvr As PanelServer
        If GetPanelServerByIndex(idx, pansvr) Then
            pansvr.info.Text = g_dbservers(idx).host & ":" & g_dbservers(idx).port & vbCrLf & _
                     "并发连接: " & clients & " - " & dbconnnums & vbCrLf & _
                     "读写操作: " & querys & vbCrLf & "写入操作: " & querys_write & vbCrLf
        End If
    End Sub

    Private Sub SetDBServerInfo(ByRef info As TextBox, ByVal idx As Integer, ByVal clients As Integer, ByVal querys As Integer, ByVal querys_write As Integer, ByVal dbconnnums As Integer)
        info.Text = g_dbservers(idx).host & ":" & g_dbservers(idx).port & vbCrLf & _
                     "并发连接: " & clients & " - " & dbconnnums & vbCrLf & _
                     "读写操作: " & querys & vbCrLf & "写入操作: " & querys_write & vbCrLf
    End Sub

    Private Sub PrintLog(ByVal strMsg As String)
        txtLog.AppendText(strMsg + vbCrLf)
    End Sub
    '根据设定的集群数据库服务器信息重新显示数据库图标状态
    Private Sub InitPannelServer()
        Dim pansvr As PanelServer

        '首先初始化所有的Pannel Server
        For idx As Integer = 0 To MAX_DBCOUNTS - 1 Step 1
            DisabledModify(idx, True)
        Next

        For idx As Integer = 0 To g_dbservers.Count - 1 Step 1
            If GetPanelServerByIndex(idx, pansvr) Then
                pansvr.picbox.BackgroundImage = ImageServer.Images(1)
                pansvr.info.TextAlign = HorizontalAlignment.Left
                SetDBServerInfo(pansvr.info, idx, 0, 0, 0, 0)
                pansvr.picdel.BackgroundImage = ImageServer.Images(4)
                pansvr.picedit.BackgroundImage = ImageServer.Images(5)
                pansvr.picbox.Tag = idx
                pansvr.pan.Tag = idx

                AddHandler pansvr.picdel.Click, AddressOf picDel_Click
                AddHandler pansvr.picedit.Click, AddressOf picEdit_Click
                AddHandler pansvr.picbox.MouseEnter, AddressOf panSever_MouseEnter
                AddHandler pansvr.pan.MouseLeave, AddressOf panSever_MouseLeave
                
                pansvr.pan.Visible = True
            End If
        Next
        If GetPanelServerByIndex(g_dbservers.Count, pansvr) Then
            pansvr.picbox.BackgroundImage = ImageServer.Images(0)
            pansvr.info.TextAlign = HorizontalAlignment.Center
            pansvr.info.Text = "请点击图标添加新的数据库服务器"
            pansvr.picdel.BackgroundImage = Nothing
            pansvr.picedit.BackgroundImage = Nothing
            pansvr.picbox.Tag = g_dbservers.Count
            pansvr.pan.Tag = g_dbservers.Count

            AddHandler pansvr.picbox.Click, AddressOf picAdd_Click

            RemoveHandler pansvr.picdel.Click, AddressOf picDel_Click
            RemoveHandler pansvr.picedit.Click, AddressOf picEdit_Click
            RemoveHandler pansvr.picbox.MouseEnter, AddressOf panSever_MouseEnter
           
            pansvr.pan.Visible = True
        End If

    End Sub
    Private sTeaKey As String = "SQlPrOxySqlProxy"       'Tea加密的Key
    'Tea加密s然后base64编码输出
    Private Function Encrypt(ByVal txt As String) As String
        If txt = "" Or txt.Length() < 8 Then Return txt
        Dim k As Byte() = System.Text.Encoding.Default.GetBytes(sTeaKey)
        Dim s As Byte() = System.Text.Encoding.Default.GetBytes(txt)
        Dim tea As HashTEA.hashtea = New HashTEA.hashtea()
        Dim b As Byte() = tea.HashTEA(s, k, 0, True)
        'encrypt:为加密前缀，为了兼容以前未加密的数据
        'Dim ret As String = "encrypt:" + Convert.ToBase64String(b)
        'MsgBox(txt + vbCrLf + ret)
        Return "encrypt:" + Convert.ToBase64String(b)
    End Function
    '解密保存的参数信息
    Private Function Decrypt(ByVal txt As String) As String
        If txt = "" Or txt.Length() < 8 Then Return txt
        '非加密字符串
        If txt.Substring(0, 8) <> "encrypt:" Then Return txt '直接返回

        txt = txt.Substring(8)
        Dim k As Byte() = System.Text.Encoding.Default.GetBytes(sTeaKey)
        Dim b As Byte() = Convert.FromBase64String(txt)
        Dim tea As HashTEA.hashtea = New HashTEA.hashtea()
        Dim s As Byte() = tea.UnHashTEA(b, k, 0, True)
        'Dim ret As String = System.Text.Encoding.Default.GetString(s)
        'MsgBox(txt + vbCrLf + ret)
        Return System.Text.Encoding.Default.GetString(s)
    End Function
    Private Sub SaveSetting()
        Dim dbsetting As String = ""
        Dim dbhost As String = ""
        Dim strAccessList As String = ""
        dbsetting = txtDBName.Text & "," & g_dbuser & "," & g_dbpswd & "," & iPort_Proxy
        If bPrivateUserName Then dbsetting = dbsetting & ",true" Else dbsetting = dbsetting & ",false"
        If bSQLSubscribe Then dbsetting = dbsetting & ",true" Else dbsetting = dbsetting & ",false"
        If bRollBack Then dbsetting = dbsetting & ",true" Else dbsetting = dbsetting & ",false"

        For idx As Integer = 0 To g_dbservers.Count - 1 Step 1
            Dim dbsvr As TDBServer = g_dbservers(idx)
            If idx = 0 Then
                dbhost = dbhost & dbsvr.host & ":" & dbsvr.port & ":" & dbsvr.dbuser & ":" & dbsvr.dbpswd
            Else
                dbhost = dbhost & "," & dbsvr.host & ":" & dbsvr.port & ":" & dbsvr.dbuser & ":" & dbsvr.dbpswd
            End If
        Next
        If frmParam.chkEnabled.Checked Then
            strAccessList = strAccessList & "true|"
        Else
            strAccessList = strAccessList & "false|"
        End If
        If frmParam.rdAccess1.Checked Then
            strAccessList = strAccessList & "true|"
        Else
            strAccessList = strAccessList & "false|"
        End If
        For idx As Integer = 0 To frmParam.lstIP.Items.Count() - 1
            strAccessList = strAccessList & frmParam.lstIP.Items(idx) & ","
        Next

        My.Settings.DBServers = Encrypt(dbhost)
        My.Settings.DBSetting = Encrypt(dbsetting)
        My.Settings.AccessList = strAccessList
    End Sub
    Private Sub LoadSetting()
        Dim dbsetting As String = Decrypt(My.Settings.DBSetting)
        Dim dbhost As String = Decrypt(My.Settings.DBServers)
        Dim strAccessList As String = My.Settings.AccessList
        Dim arrSetting() As String = dbsetting.Split(",")
        If arrSetting.Length > 0 Then txtDBName.Text = arrSetting(0)
        If arrSetting.Length > 1 Then g_dbuser = "" ' arrSetting(1)
        If arrSetting.Length > 2 Then g_dbpswd = "" ' arrSetting(2)
        If arrSetting.Length > 3 Then iPort_Proxy = Convert.ToInt32(arrSetting(3))
        If arrSetting.Length > 4 Then
            If arrSetting(4) = "true" Then bPrivateUserName = True
        End If
        If arrSetting.Length > 5 Then
            If arrSetting(5) = "true" Then bSQLSubscribe = True
        End If
        If arrSetting.Length > 6 Then
            If arrSetting(6) = "true" Then bRollBack = True
        End If
        chkRollback.Checked = bRollBack

        If iPort_Proxy < 1 Then iPort_Proxy = 1433
        txtSvrPort.Text = iPort_Proxy
        InitDBServers(dbhost)
    End Sub

    Private Sub ViewProxyInfo()
        Dim pansvr As PanelServer
        Dim iPrintnums As Integer = GetDBStatus(-8, 0)
        'Dim iLoadMode As Integer = GetDBStatus(-10, 0)  '当前工作模式
        lblPrintNums.Text = "(" & iPrintnums & ")"
        For idx As Integer = 0 To g_dbservers.Count - 1 Step 1
            If GetPanelServerByIndex(idx, pansvr) Then
                Dim iValid As Integer = GetDBStatus(idx, 0)
                Dim clients As Integer = GetDBStatus(idx, 1)
                Dim dbconnnums As Integer = GetDBStatus(idx, 2)
                Dim querys As Integer = GetDBStatus(idx, 3)
                Dim querys_write As Integer = GetDBStatus(idx, 4) '写入操作次数

                If iValid = 0 Then '有效正常
                    pansvr.picbox.BackgroundImage = ImageServer.Images(3)
                ElseIf iValid = 1 Then    '1数据库连接错误
                    pansvr.picbox.BackgroundImage = ImageServer.Images(2)
                ElseIf iValid = 2 Then    '2数据库同步异常
                    pansvr.picbox.BackgroundImage = ImageServer.Images(6)
                End If
                SetDBServerInfo(pansvr.info, idx, clients, querys, querys_write, dbconnnums)
            End If
        Next
    End Sub
    Private Sub Proxy_Close()
        'SetLogLevel(0, LOGLEVEL_DEBUG) ‘StopProxy已作保护
        StopProxy() 'StopProxy会阻塞直到所有线程关闭，在阻塞过程中会打印输出Client关闭信息，此时因frmain界面主线程等待StopProxy返回阻塞()，导致打印输出阻塞因此死锁。
        SetControlStatus(False)
        ViewProxyInfo()
    End Sub
    '如果允许集群数据库采用不同的帐号密码访问，则检查是否所有的数据库服务已经设置了帐号密码
    Private Function CheckUserAndPswd() As Boolean
        If Not bPrivateUserName Then Return True
        For idx As Integer = 0 To g_dbservers.Count - 1 Step 1
            Dim dbsvr As TDBServer = g_dbservers(idx)
            If dbsvr.dbuser = "" Then Return False
        Next
        Return True
    End Function

    Private Sub Proxy_Start()
        
        If Not CheckUserAndPswd() Then
            MsgBox("你选择了 '允许集群数据库采用不同的帐号密码访问'" & vbCrLf & "因此请分别设置每个集群数据库的访问帐号和密码")
            Return
        End If

        Dim szParam As String = ""
        Dim dbhost As String = ""
        If rdMode0.Checked Then
            szParam = szParam & "mode=master "
        ElseIf rdMode1.Checked Then
            szParam = szParam & "mode=backup "
        Else
            szParam = szParam & "mode=rand "
        End If
        If bSQLSubscribe Then
            szParam = szParam & "waitreply=-1 "
        Else
            szParam = szParam & "waitreply=0 "
        End If
        If bRollBack Then
            szParam = szParam & "rollback=true syncond=true "
        Else
            szParam = szParam & "rollback=0 syncond=false "
        End If
        
        szParam = szParam & "dbname=" & txtDBName.Text & " "
        If g_dbuser <> "" And Not bPrivateUserName Then
            szParam = szParam & "dbuser=" & g_dbuser & " "
            szParam = szParam & "dbpswd=" & g_dbpswd & " "
        End If

        For idx As Integer = 0 To g_dbservers.Count - 1 Step 1

            Dim dbsvr As TDBServer = g_dbservers(idx)
            Dim dbuser As String = ""
            If bPrivateUserName Then dbuser = ":" & dbsvr.dbuser & ":" & dbsvr.dbpswd
            If idx = 0 Then
                dbhost = dbhost & dbsvr.host & ":" & dbsvr.port & dbuser
            Else
                dbhost = dbhost & "," & dbsvr.host & ":" & dbsvr.port & dbuser
            End If
        Next
        szParam = szParam & "dbhost=" & dbhost & " "
        SetParameters(1, szParam)

        Dim iport As Integer = 0
        If txtSvrPort.Text <> "" Then iport = Convert.ToInt32(txtSvrPort.Text)
        If iport > 0 Then iPort_Proxy = iport

        If StartProxy(iPort_Proxy) Then
            SetControlStatus(True)
            txtSvrPort.Text = iPort_Proxy
        End If

    End Sub

    Private Sub NotifyIcon1_MouseClick(ByVal sender As Object, ByVal e As System.Windows.Forms.MouseEventArgs) Handles NotifyIcon1.MouseDoubleClick
        Me.WindowState = FormWindowState.Normal
        Me.ShowInTaskbar = True
        NotifyIcon1.Visible = False
    End Sub

    Private Sub frmMain_FormClosing(ByVal sender As Object, ByVal e As System.Windows.Forms.FormClosingEventArgs) Handles Me.FormClosing
        SaveSetting()
        If bStartProxy Then
            e.Cancel = True
            Me.WindowState = FormWindowState.Minimized
            Me.ShowInTaskbar = False
            NotifyIcon1.Visible = True
        Else
            Proxy_Close()
            frmPrintSQL.Close()
        End If
    End Sub

    Private Sub frmMain_Load(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles MyBase.Load
        NotifyIcon1.Icon = My.Resources.SQLProxy
        Me.Icon = My.Resources.stopi
        LoadSetting()
        InitPannelServer()
        Me.Width = 705
        gpSettings.Left = gpLogs.Left
        gpSettings.Top = gpLogs.Top
        '设置默认日志输出
        rdLog_CheckedChanged(sender, e)
    End Sub

   
    Private Sub panSever_MouseEnter(ByVal sender As Object, ByVal e As System.EventArgs)
        Dim pansvr As PanelServer
        Dim idx As Integer = sender.tag
        If bStartProxy Then Exit Sub

        If GetPanelServerByIndex(idx, pansvr) Then
            pansvr.picdel.Visible = True
            pansvr.picedit.Visible = True
        End If
    End Sub
    Private Sub panSever_MouseLeave(ByVal sender As Object, ByVal e As System.EventArgs)
        Dim pansvr As PanelServer
        Dim idx As Integer = sender.tag
        If GetPanelServerByIndex(idx, pansvr) Then
            pansvr.picdel.Visible = False
            pansvr.picedit.Visible = False
        End If
    End Sub

    Private Sub picDel_Click(ByVal sender As Object, ByVal e As System.EventArgs)
        If bStartProxy Then Exit Sub

        Dim szName As String = sender.name.ToString
        Dim idx As Integer = Convert.ToInt32(szName.Substring(6))
        If idx > 0 And idx <= g_dbservers.Count Then
            DisabledModify(idx - 1, False)
            Dim dbsvr As TDBServer = g_dbservers(idx - 1)
            Dim sinfo As String = "是否确认删除 " & dbsvr.host & ":" & dbsvr.port & " ?"
            If MessageBox.Show(sinfo, "删除确认", MessageBoxButtons.YesNo, MessageBoxIcon.Question, MessageBoxDefaultButton.Button2) = Windows.Forms.DialogResult.Yes Then
                g_dbservers.RemoveAt(idx - 1)
            End If
            InitPannelServer()
        End If
    End Sub
    Private Sub picAdd_Click(ByVal sender As Object, ByVal e As System.EventArgs)
        If bStartProxy Then Exit Sub

        frmAdd.txtHost.Text = ""
        frmAdd.txtPort.Text = 1433
        frmAdd.btnOK.Text = "添加"
        Dim dr As DialogResult = frmAdd.ShowDialog(Me)
        If dr = Windows.Forms.DialogResult.OK Then
            Dim szDBinfo As String = frmAdd.txtHost.Text & ":" & frmAdd.txtPort.Text
            If bPrivateUserName Then szDBinfo = szDBinfo & ":" & frmAdd.txtuser.Text & ":" & frmAdd.txtpswd.Text
            AddDBServer(szDBinfo)
            InitPannelServer()
        End If
    End Sub
    Private Sub picEdit_Click(ByVal sender As Object, ByVal e As System.EventArgs)
        If bStartProxy Then Exit Sub

        Dim pansvr As PanelServer
        Dim szName As String = sender.name.ToString
        Dim idx As Integer = Convert.ToInt32(szName.Substring(7))
        If idx > 0 And idx <= g_dbservers.Count Then
            Dim dbsvr As TDBServer = g_dbservers(idx - 1)
            If GetPanelServerByIndex(idx - 1, pansvr) Then
                pansvr.picdel.Visible = False
                pansvr.picedit.Visible = False
                RemoveHandler pansvr.picdel.Click, AddressOf picDel_Click
                RemoveHandler pansvr.picedit.Click, AddressOf picEdit_Click

                frmAdd.txtHost.Text = dbsvr.host
                frmAdd.txtPort.Text = dbsvr.port
                frmAdd.txtuser.Text = dbsvr.dbuser
                frmAdd.txtpswd.Text = dbsvr.dbpswd
                frmAdd.btnOK.Text = "修改"
                Dim dr As DialogResult = frmAdd.ShowDialog(Me)
                If dr = Windows.Forms.DialogResult.OK Then
                    dbsvr.host = frmAdd.txtHost.Text
                    dbsvr.port = frmAdd.txtPort.Text
                    dbsvr.dbuser = frmAdd.txtuser.Text
                    dbsvr.dbpswd = frmAdd.txtpswd.Text
                    g_dbservers(idx - 1) = dbsvr
                    SetDBServerInfo(idx - 1, 0, 0, 0, 0)
                End If

                AddHandler pansvr.picdel.Click, AddressOf picDel_Click
                AddHandler pansvr.picedit.Click, AddressOf picEdit_Click
            End If
        End If
    End Sub

    Private Sub rdLog_CheckedChanged(ByVal sender As System.Object, ByVal e As System.EventArgs) _
    Handles rdLog0.CheckedChanged, rdLog2.CheckedChanged, rdLog3.CheckedChanged

        If rdLog0.Checked Then
            SetLogLevel(txtLog.Handle, LOGLEVEL_DEBUG)
        ElseIf rdLog2.Checked Then
            SetLogLevel(txtLog.Handle, LOGLEVEL_INFO)
        ElseIf rdLog3.Checked Then
            SetLogLevel(txtLog.Handle, LOGLEVEL_ERROR)
        End If
    End Sub

    Private Sub Timer1_Tick(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles Timer1.Tick
        ViewProxyInfo()
    End Sub

    '根据启动状态设置控件使能状态
    Private Sub SetControlStatus(ByVal bStarted As Boolean)
        bStartProxy = bStarted
        gpParam.Enabled = Not bStarted
        gpDBInfo.Enabled = Not bStarted
        lnkTest.Enabled = Not bStarted
        lnkParam.Enabled = Not bStarted
        Timer1.Enabled = bStarted

        Dim pansvr As PanelServer
        If GetPanelServerByIndex(g_dbservers.Count, pansvr) Then
            pansvr.pan.Visible = Not bStarted
        End If
        If bStarted Then
            btnStart.BackgroundImage = My.Resources.p_stop
            Me.Icon = My.Resources.playi
            'btnStart.Text = "停止"
        Else
            btnStart.BackgroundImage = My.Resources.p_play
            Me.Icon = My.Resources.stopi
            'btnStart.Text = "启动"
        End If
    End Sub

    Private Sub btnStart_Click(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles btnStart.Click
        btnStart.Enabled = False
        If bStartProxy Then
            Proxy_Close()
        Else
            Proxy_Start()
        End If
        btnStart.Enabled = True
    End Sub
    Private Sub btnStart_MouseDoubleClick(ByVal sender As Object, ByVal e As System.Windows.Forms.MouseEventArgs) _
    Handles btnStart.MouseUp
        If e.Button = Windows.Forms.MouseButtons.Right And (Control.ModifierKeys And Keys.Alt) Then
            Dim inputstr As String = InputBox("请输入可选运行控制参数：", "运行参数")
            If inputstr <> "" Then SetParameters(1, inputstr)
        End If
    End Sub
    Private Sub lnkClear_LinkClicked(ByVal sender As System.Object, ByVal e As System.Windows.Forms.LinkLabelLinkClickedEventArgs) Handles lnkClear.LinkClicked
        txtLog.Text = ""
    End Sub

    Private Sub TabControl1_Selected(ByVal sender As Object, ByVal e As System.Windows.Forms.TabControlEventArgs) Handles TabControl1.Selected
        If e.TabPageIndex = 1 Then
            gpLogs.Visible = True
            gpSettings.Visible = False
        Else
            gpLogs.Visible = False
            gpSettings.Visible = True
        End If
    End Sub

    Private Sub LinkLabel1_LinkClicked(ByVal sender As System.Object, ByVal e As System.Windows.Forms.LinkLabelLinkClickedEventArgs) Handles LinkLabel1.LinkClicked
        System.Diagnostics.Process.Start(LinkLabel1.Text)
    End Sub

    
    Private Sub lnkLog_LinkClicked(ByVal sender As System.Object, ByVal e As System.Windows.Forms.LinkLabelLinkClickedEventArgs) Handles lnkLog.LinkClicked
        frmPrintSQL.Show(Me)
    End Sub

    Private Sub lnkParam_LinkClicked(ByVal sender As System.Object, ByVal e As System.Windows.Forms.LinkLabelLinkClickedEventArgs) Handles lnkParam.LinkClicked
        frmParam.ShowDialog(Me)
    End Sub

    Private Sub ModeCheckedChanged(ByVal sender As System.Object, ByVal e As System.EventArgs) _
    Handles rdMode0.CheckedChanged, rdMode1.CheckedChanged
        If rdMode0.Checked Then
            txtModeInfo.Text = "负载均衡 1:" & vbCrLf & _
                               "每个SQL客户端固定连接第一个有效的服务器为主服务器，其它服务器为负载服务器。" & vbCrLf & _
                               "如果是查询操作则随机选择一台负载服务器执行; 如果是写入操作且执行成功则实时同步到其它负载服务器。"
        ElseIf rdMode1.Checked Then
            txtModeInfo.Text = "容灾备份:" & vbCrLf & _
                               "每个SQL客户端固定连接第一个有效的服务器为主服务器，其它服务器为备份服务器。" & vbCrLf & _
                               "如果是写入操作且执行成功则实时同步到其它备份服务器; 备份服务器不分但负载压力。"
        ElseIf rdMode2.Checked Then
            txtModeInfo.Text = "负载均衡 2:" & vbCrLf & _
                               "每个SQL客户端随机连接一台有效的服务器为主服务器，其它服务器为负载服务器。" & vbCrLf & _
                               "如果是写入操作且执行成功,则实时同步到其它负载服务器。此模式不支持采用SQL发布订阅方式同步数据。"
        End If
    End Sub

    Private Sub ExecQueryFile(ByVal filename() As String)
        For i = 0 To filename.Length - 1 Step 1
            Dim fname As String = filename(0)
            ExecFileData(1, fname)
        Next
    End Sub
    Private Sub File_DragDrop(ByVal sender As Object, ByVal e As System.Windows.Forms.DragEventArgs) Handles txtLog.DragDrop
        Dim filename() As String = e.Data.GetData(DataFormats.FileDrop) 'GetData("FileNameW", True)
        If bStartProxy Then Return
        '此函数会开启独立工作线程进行日志输出到本UI的txtlog窗体
        '因此在本UI线程直接调用，则会产生死锁，需要开启独立的线程调用
        Dim objparam As New Object
        objparam = filename
        Dim td As New Threading.Thread(AddressOf ExecQueryFile)
        td.Start(objparam)
    End Sub

    Private Sub File_DragOver(ByVal sender As Object, ByVal e As System.Windows.Forms.DragEventArgs) Handles txtLog.DragOver
        If e.Data.GetDataPresent(DataFormats.FileDrop) And Not bStartProxy Then
            e.Effect = DragDropEffects.Link
        Else
            e.Effect = DragDropEffects.None
        End If
    End Sub

    Private Sub txtLog_TextChanged(ByVal sender As Object, ByVal e As System.EventArgs) Handles txtLog.TextChanged
        If txtLog.Text.Length > txtLog.MaxLength - 10240 Then
            txtLog.SelectionStart() = 0
            txtLog.SelectionLength() = 20480
            txtLog.SelectedText = ""
        End If
    End Sub

    Private Sub lnkTest_LinkClicked(ByVal sender As System.Object, ByVal e As System.Windows.Forms.LinkLabelLinkClickedEventArgs) _
    Handles lnkTest.LinkClicked

        Dim username As String = ""
        Dim userpswd As String = ""
        If bStartProxy Then Exit Sub
        If Not bPrivateUserName Then
            Dim inputstr As String = InputBox("请输入数据库'" & txtDBName.Text & "' 的访问帐号和密码,譬如 sa:1234 ", "连接测试帐号密码")
            If inputstr = "" Then Return
            Dim arr() As String = inputstr.Split(":")
            username = arr(0)
            If arr.Length > 1 Then userpswd = arr(1)
        ElseIf Not CheckUserAndPswd() Then
            MsgBox("你选择了 '允许集群数据库采用不同的帐号密码访问'" & vbCrLf & "因此请分别设置每个集群数据库的访问帐号和密码")
            Return
        End If

        Dim pansvr As PanelServer
        For idx As Integer = 0 To g_dbservers.Count - 1 Step 1
            If GetPanelServerByIndex(idx, pansvr) Then
                Dim dbsvr As TDBServer = g_dbservers(idx)
                Dim dbname As String = txtDBName.Text
                Dim dbuser As String = "" 'txtDBUser.Text
                Dim dbpswd As String = "" 'txtDBPswd.Text
                If bPrivateUserName Then dbuser = dbsvr.dbuser Else dbuser = username
                If bPrivateUserName Then dbpswd = dbsvr.dbpswd Else dbpswd = userpswd

                Dim iret As Integer = DBTest(dbsvr.host, dbsvr.port, dbuser, dbpswd, dbname)
                If iret = 0 Then
                    pansvr.picbox.BackgroundImage = ImageServer.Images(3)
                Else
                    pansvr.picbox.BackgroundImage = ImageServer.Images(2)
                End If
            End If
        Next
    End Sub

    Private Sub chkRollback_CheckedChanged(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles chkRollback.CheckedChanged
        bRollBack = chkRollback.Checked
    End Sub
End Class

