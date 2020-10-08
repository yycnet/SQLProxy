Public Class frmParam

    '解析访问控制列表，格式 <是否启用访问控制>|<允许禁止列表ip访问>|<ip,ip,ip...>
    Private Sub InitAccessAuth(ByVal strAccessList As String)
        Dim bEnabled As Boolean = False
        Dim bAccess As Boolean = True
        Dim striplist As String = ""
        If strAccessList <> "" Then
            Dim arrAccess() As String = strAccessList.Split("|")
            If arrAccess.Length() >= 3 Then
                If arrAccess(0) = "true" Or arrAccess(0) = "1" Then
                    bEnabled = True
                Else
                    bEnabled = False
                End If
                If arrAccess(1) = "true" Or arrAccess(1) = "1" Then
                    bAccess = True
                Else
                    bAccess = False
                End If
                striplist = arrAccess(2)
            End If
        End If

        chkEnabled.Checked = bEnabled
        rdAccess1.Checked = bAccess
        lstIP.Items.Clear()
        If striplist <> "" Then
            Dim arrIP() As String = striplist.Split(",")
            For i As Integer = 0 To arrIP.Length() - 1 Step 1
                If arrIP(i) <> "" Then lstIP.Items.Add(arrIP(i))
            Next
        End If
    End Sub

    Private Sub LoadSetting()
        Dim strAccessList As String = My.Settings.AccessList 
        InitAccessAuth(strAccessList)
    End Sub
    Private Sub SaveSetting()
        Dim strAccessList As String = ""
        If chkEnabled.Checked Then
            strAccessList = strAccessList & "true|"
        Else
            strAccessList = strAccessList & "false|"
        End If
        If rdAccess1.Checked Then
            strAccessList = strAccessList & "true|"
        Else
            strAccessList = strAccessList & "false|"
        End If
        For idx As Integer = 0 To lstIP.Items.Count() - 1
            strAccessList = strAccessList & lstIP.Items(idx) & ","
        Next
        My.Settings.AccessList = strAccessList
    End Sub

    Private Sub frmParam_Load(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles MyBase.Load

        chkUserPswd.Checked = frmMain.bPrivateUserName
        chkSubscribe.Checked = frmMain.bSQLSubscribe
       
        chkUserPswd.Enabled = True
        chkSubscribe.Enabled = True
        LoadSetting()
    End Sub

    Private Sub btnOK_Click(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles btnOK.Click
        Dim strAccesslist As String = "accesslist="
        If chkEnabled.Checked Then
            If rdAccess1.Checked Then
                strAccesslist = strAccesslist & "1|"
            Else
                strAccesslist = strAccesslist & "0|"
            End If
            For idx As Integer = 0 To lstIP.Items.Count() - 1
                strAccesslist = strAccesslist & lstIP.Items(idx) & ","
            Next
        End If
        SetParameters(1, strAccesslist)
        SaveSetting()
        
        frmMain.bPrivateUserName = chkUserPswd.Checked
        frmMain.bSQLSubscribe = chkSubscribe.Checked
        Me.Close()
    End Sub

    Private Sub Button_Click(ByVal sender As System.Object, ByVal e As System.EventArgs) _
    Handles btnAdd.Click, btnDel.Click, btnClear.Click
        If sender.name.ToString = "btnClear" Then
            lstIP.Items.Clear()
        ElseIf sender.name.ToString = "btnAdd" Then
            lstIP.Items.Add(txtClntIP.Text)
        ElseIf sender.name.ToString = "btnDel" Then
            lstIP.Items.Remove(txtClntIP.Text)
        End If

    End Sub

    Private Sub ListBox1_SelectedIndexChanged(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles lstIP.SelectedIndexChanged
        txtClntIP.Text = lstIP.SelectedItem
    End Sub

    Private Sub txtSvrPort_KeyPress(ByVal sender As Object, ByVal e As System.Windows.Forms.KeyPressEventArgs)
        If Char.IsDigit(e.KeyChar) Or e.KeyChar = Chr(8) Then
            e.Handled = False
        Else
            e.Handled = True
        End If
    End Sub
End Class