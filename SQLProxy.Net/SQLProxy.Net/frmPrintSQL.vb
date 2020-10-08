Public Class frmPrintSQL

    Private Sub btnOK_Click(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles btnOK.Click
        Dim szFilter As String
        Dim iType As Integer = 0
        Dim iStaExec As Integer = 0
        Dim iMsExec As Integer = 0
        If txtMsExec.Text <> "" Then iMsExec = Convert.ToInt32(txtMsExec.Text)
        If chkType1.Checked Then iType = iType Or 1
        If chkType2.Checked Then iType = iType Or 2
        If chkType3.Checked Then iType = iType Or 4
        If iType = 0 Then
            chkType1.Checked = chkType2.Checked = chkType3.Checked = True
        End If
        If chkStaExec1.Checked Then
            iStaExec = 0
        ElseIf chkStaExec2.Checked Then
            iStaExec = 1
        ElseIf chkStaExec3.Checked Then
            iStaExec = 2
        End If

        If chkPrintSQL.Checked Then
            szFilter = "sqltype=" & iType & " staexec=" & iStaExec & " msexec=" & iMsExec & " ipfilter=" & txtIP.Text & " sqlfilter=""" & txtFilter.Text & """"
        Else
            szFilter = ""
        End If
        SetParameters(2, szFilter)
        'Me.Close()
    End Sub

    Private Sub chkPrintSQL_CheckedChanged(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles chkPrintSQL.CheckedChanged
        If chkPrintSQL.Checked Then
            GroupBox1.Enabled = True
            GroupBox2.Enabled = True
            txtMsExec.Enabled = True
            txtIP.Enabled = True
            txtFilter.Enabled = True
        Else
            GroupBox1.Enabled = False
            GroupBox2.Enabled = False
            txtMsExec.Enabled = False
            txtIP.Enabled = False
            txtFilter.Enabled = False
            Dim szFilter As String = ""
            SetParameters(2, szFilter)
        End If
    End Sub

    Private Sub frmPrintSQL_FormClosing(ByVal sender As Object, ByVal e As System.Windows.Forms.FormClosingEventArgs) Handles Me.FormClosing
        If Me.Visible Then
            Me.Hide()
            e.Cancel = True
        End If
    End Sub

    Private Sub txtMsExec_KeyPress(ByVal sender As Object, ByVal e As System.Windows.Forms.KeyPressEventArgs) Handles txtMsExec.KeyPress
        If Char.IsDigit(e.KeyChar) Or e.KeyChar = Chr(8) Then
            e.Handled = False
        Else
            e.Handled = True
        End If
    End Sub

End Class