Public Class frmAdd

    Private Sub btnOK_Click(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles btnOK.Click
        Me.DialogResult = Windows.Forms.DialogResult.OK
        Me.Close()
    End Sub

    Private Sub frmAdd_Load(ByVal sender As Object, ByVal e As System.EventArgs) Handles Me.Load
        If frmMain.bPrivateUserName Then
            Me.Height = 140
        Else
            Me.Height = 110
        End If
    End Sub
End Class