<Global.Microsoft.VisualBasic.CompilerServices.DesignerGenerated()> _
Partial Class frmParam
    Inherits System.Windows.Forms.Form

    'Form 重写 Dispose，以清理组件列表。
    <System.Diagnostics.DebuggerNonUserCode()> _
    Protected Overrides Sub Dispose(ByVal disposing As Boolean)
        Try
            If disposing AndAlso components IsNot Nothing Then
                components.Dispose()
            End If
        Finally
            MyBase.Dispose(disposing)
        End Try
    End Sub

    'Windows 窗体设计器所必需的
    Private components As System.ComponentModel.IContainer

    '注意: 以下过程是 Windows 窗体设计器所必需的
    '可以使用 Windows 窗体设计器修改它。
    '不要使用代码编辑器修改它。
    <System.Diagnostics.DebuggerStepThrough()> _
    Private Sub InitializeComponent()
        Me.GroupBox1 = New System.Windows.Forms.GroupBox
        Me.btnClear = New System.Windows.Forms.Button
        Me.btnDel = New System.Windows.Forms.Button
        Me.btnAdd = New System.Windows.Forms.Button
        Me.txtClntIP = New System.Windows.Forms.TextBox
        Me.Label3 = New System.Windows.Forms.Label
        Me.chkEnabled = New System.Windows.Forms.CheckBox
        Me.lstIP = New System.Windows.Forms.ListBox
        Me.Label2 = New System.Windows.Forms.Label
        Me.rdAccess2 = New System.Windows.Forms.RadioButton
        Me.rdAccess1 = New System.Windows.Forms.RadioButton
        Me.btnOK = New System.Windows.Forms.Button
        Me.GroupBox2 = New System.Windows.Forms.GroupBox
        Me.chkSubscribe = New System.Windows.Forms.CheckBox
        Me.chkUserPswd = New System.Windows.Forms.CheckBox
        Me.GroupBox1.SuspendLayout()
        Me.GroupBox2.SuspendLayout()
        Me.SuspendLayout()
        '
        'GroupBox1
        '
        Me.GroupBox1.Controls.Add(Me.btnClear)
        Me.GroupBox1.Controls.Add(Me.btnDel)
        Me.GroupBox1.Controls.Add(Me.btnAdd)
        Me.GroupBox1.Controls.Add(Me.txtClntIP)
        Me.GroupBox1.Controls.Add(Me.Label3)
        Me.GroupBox1.Controls.Add(Me.chkEnabled)
        Me.GroupBox1.Controls.Add(Me.lstIP)
        Me.GroupBox1.Controls.Add(Me.Label2)
        Me.GroupBox1.Controls.Add(Me.rdAccess2)
        Me.GroupBox1.Controls.Add(Me.rdAccess1)
        Me.GroupBox1.Location = New System.Drawing.Point(13, 7)
        Me.GroupBox1.Name = "GroupBox1"
        Me.GroupBox1.Size = New System.Drawing.Size(264, 190)
        Me.GroupBox1.TabIndex = 0
        Me.GroupBox1.TabStop = False
        '
        'btnClear
        '
        Me.btnClear.Location = New System.Drawing.Point(161, 142)
        Me.btnClear.Name = "btnClear"
        Me.btnClear.Size = New System.Drawing.Size(67, 23)
        Me.btnClear.TabIndex = 9
        Me.btnClear.Text = "清空列表"
        Me.btnClear.UseVisualStyleBackColor = True
        '
        'btnDel
        '
        Me.btnDel.Location = New System.Drawing.Point(162, 84)
        Me.btnDel.Name = "btnDel"
        Me.btnDel.Size = New System.Drawing.Size(37, 23)
        Me.btnDel.TabIndex = 8
        Me.btnDel.Text = "删除"
        Me.btnDel.UseVisualStyleBackColor = True
        '
        'btnAdd
        '
        Me.btnAdd.Location = New System.Drawing.Point(222, 84)
        Me.btnAdd.Name = "btnAdd"
        Me.btnAdd.Size = New System.Drawing.Size(37, 23)
        Me.btnAdd.TabIndex = 7
        Me.btnAdd.Text = "新增"
        Me.btnAdd.UseVisualStyleBackColor = True
        '
        'txtClntIP
        '
        Me.txtClntIP.Location = New System.Drawing.Point(161, 57)
        Me.txtClntIP.Name = "txtClntIP"
        Me.txtClntIP.Size = New System.Drawing.Size(98, 21)
        Me.txtClntIP.TabIndex = 6
        '
        'Label3
        '
        Me.Label3.AutoSize = True
        Me.Label3.Location = New System.Drawing.Point(163, 42)
        Me.Label3.Name = "Label3"
        Me.Label3.Size = New System.Drawing.Size(65, 12)
        Me.Label3.TabIndex = 5
        Me.Label3.Text = "客户端IP："
        '
        'chkEnabled
        '
        Me.chkEnabled.AutoSize = True
        Me.chkEnabled.Location = New System.Drawing.Point(10, 16)
        Me.chkEnabled.Name = "chkEnabled"
        Me.chkEnabled.Size = New System.Drawing.Size(132, 16)
        Me.chkEnabled.TabIndex = 4
        Me.chkEnabled.Text = "启用客户端访问控制"
        Me.chkEnabled.UseVisualStyleBackColor = True
        '
        'lstIP
        '
        Me.lstIP.FormattingEnabled = True
        Me.lstIP.ItemHeight = 12
        Me.lstIP.Location = New System.Drawing.Point(9, 41)
        Me.lstIP.Name = "lstIP"
        Me.lstIP.Size = New System.Drawing.Size(146, 124)
        Me.lstIP.TabIndex = 3
        '
        'Label2
        '
        Me.Label2.AutoSize = True
        Me.Label2.Location = New System.Drawing.Point(115, 171)
        Me.Label2.Name = "Label2"
        Me.Label2.Size = New System.Drawing.Size(137, 12)
        Me.Label2.TabIndex = 2
        Me.Label2.Text = "列表中客户端访问本服务"
        '
        'rdAccess2
        '
        Me.rdAccess2.AutoSize = True
        Me.rdAccess2.Location = New System.Drawing.Point(62, 169)
        Me.rdAccess2.Name = "rdAccess2"
        Me.rdAccess2.Size = New System.Drawing.Size(47, 16)
        Me.rdAccess2.TabIndex = 1
        Me.rdAccess2.Text = "禁止"
        Me.rdAccess2.UseVisualStyleBackColor = True
        '
        'rdAccess1
        '
        Me.rdAccess1.AutoSize = True
        Me.rdAccess1.Checked = True
        Me.rdAccess1.Location = New System.Drawing.Point(9, 169)
        Me.rdAccess1.Name = "rdAccess1"
        Me.rdAccess1.Size = New System.Drawing.Size(47, 16)
        Me.rdAccess1.TabIndex = 0
        Me.rdAccess1.TabStop = True
        Me.rdAccess1.Text = "允许"
        Me.rdAccess1.UseVisualStyleBackColor = True
        '
        'btnOK
        '
        Me.btnOK.Location = New System.Drawing.Point(299, 227)
        Me.btnOK.Name = "btnOK"
        Me.btnOK.Size = New System.Drawing.Size(55, 33)
        Me.btnOK.TabIndex = 2
        Me.btnOK.Text = "应用"
        Me.btnOK.UseVisualStyleBackColor = True
        '
        'GroupBox2
        '
        Me.GroupBox2.Controls.Add(Me.chkSubscribe)
        Me.GroupBox2.Controls.Add(Me.chkUserPswd)
        Me.GroupBox2.Location = New System.Drawing.Point(13, 203)
        Me.GroupBox2.Name = "GroupBox2"
        Me.GroupBox2.Size = New System.Drawing.Size(264, 62)
        Me.GroupBox2.TabIndex = 3
        Me.GroupBox2.TabStop = False
        Me.GroupBox2.Text = "高级设置"
        '
        'chkSubscribe
        '
        Me.chkSubscribe.AutoSize = True
        Me.chkSubscribe.Enabled = False
        Me.chkSubscribe.Location = New System.Drawing.Point(10, 39)
        Me.chkSubscribe.Name = "chkSubscribe"
        Me.chkSubscribe.Size = New System.Drawing.Size(246, 16)
        Me.chkSubscribe.TabIndex = 1
        Me.chkSubscribe.Text = "采用MSSQL发布订阅机制进行集群数据同步"
        Me.chkSubscribe.UseVisualStyleBackColor = True
        '
        'chkUserPswd
        '
        Me.chkUserPswd.AutoSize = True
        Me.chkUserPswd.Enabled = False
        Me.chkUserPswd.Location = New System.Drawing.Point(10, 17)
        Me.chkUserPswd.Name = "chkUserPswd"
        Me.chkUserPswd.Size = New System.Drawing.Size(240, 16)
        Me.chkUserPswd.TabIndex = 0
        Me.chkUserPswd.Text = "允许集群数据库采用不同的帐号密码访问"
        Me.chkUserPswd.UseVisualStyleBackColor = True
        '
        'frmParam
        '
        Me.AutoScaleDimensions = New System.Drawing.SizeF(6.0!, 12.0!)
        Me.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font
        Me.ClientSize = New System.Drawing.Size(366, 271)
        Me.Controls.Add(Me.GroupBox2)
        Me.Controls.Add(Me.btnOK)
        Me.Controls.Add(Me.GroupBox1)
        Me.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedSingle
        Me.MaximizeBox = False
        Me.MinimizeBox = False
        Me.Name = "frmParam"
        Me.ShowInTaskbar = False
        Me.StartPosition = System.Windows.Forms.FormStartPosition.CenterParent
        Me.Text = "参数设置"
        Me.GroupBox1.ResumeLayout(False)
        Me.GroupBox1.PerformLayout()
        Me.GroupBox2.ResumeLayout(False)
        Me.GroupBox2.PerformLayout()
        Me.ResumeLayout(False)

    End Sub
    Friend WithEvents GroupBox1 As System.Windows.Forms.GroupBox
    Friend WithEvents lstIP As System.Windows.Forms.ListBox
    Friend WithEvents Label2 As System.Windows.Forms.Label
    Friend WithEvents rdAccess2 As System.Windows.Forms.RadioButton
    Friend WithEvents rdAccess1 As System.Windows.Forms.RadioButton
    Friend WithEvents btnOK As System.Windows.Forms.Button
    Friend WithEvents txtClntIP As System.Windows.Forms.TextBox
    Friend WithEvents Label3 As System.Windows.Forms.Label
    Friend WithEvents chkEnabled As System.Windows.Forms.CheckBox
    Friend WithEvents btnClear As System.Windows.Forms.Button
    Friend WithEvents btnDel As System.Windows.Forms.Button
    Friend WithEvents btnAdd As System.Windows.Forms.Button
    Friend WithEvents GroupBox2 As System.Windows.Forms.GroupBox
    Friend WithEvents chkUserPswd As System.Windows.Forms.CheckBox
    Friend WithEvents chkSubscribe As System.Windows.Forms.CheckBox
End Class
