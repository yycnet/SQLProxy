<Global.Microsoft.VisualBasic.CompilerServices.DesignerGenerated()> _
Partial Class frmPrintSQL
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
        Me.Label1 = New System.Windows.Forms.Label
        Me.btnOK = New System.Windows.Forms.Button
        Me.chkPrintSQL = New System.Windows.Forms.CheckBox
        Me.Label4 = New System.Windows.Forms.Label
        Me.txtMsExec = New System.Windows.Forms.TextBox
        Me.Label5 = New System.Windows.Forms.Label
        Me.GroupBox2 = New System.Windows.Forms.GroupBox
        Me.chkStaExec3 = New System.Windows.Forms.RadioButton
        Me.chkStaExec2 = New System.Windows.Forms.RadioButton
        Me.chkStaExec1 = New System.Windows.Forms.RadioButton
        Me.Label2 = New System.Windows.Forms.Label
        Me.Label3 = New System.Windows.Forms.Label
        Me.txtFilter = New System.Windows.Forms.TextBox
        Me.Label6 = New System.Windows.Forms.Label
        Me.txtIP = New System.Windows.Forms.TextBox
        Me.chkType1 = New System.Windows.Forms.CheckBox
        Me.chkType3 = New System.Windows.Forms.CheckBox
        Me.chkType2 = New System.Windows.Forms.CheckBox
        Me.GroupBox1.SuspendLayout()
        Me.GroupBox2.SuspendLayout()
        Me.SuspendLayout()
        '
        'GroupBox1
        '
        Me.GroupBox1.Controls.Add(Me.chkType2)
        Me.GroupBox1.Controls.Add(Me.chkType3)
        Me.GroupBox1.Controls.Add(Me.chkType1)
        Me.GroupBox1.Controls.Add(Me.Label1)
        Me.GroupBox1.Enabled = False
        Me.GroupBox1.Location = New System.Drawing.Point(5, 25)
        Me.GroupBox1.Name = "GroupBox1"
        Me.GroupBox1.Size = New System.Drawing.Size(271, 38)
        Me.GroupBox1.TabIndex = 15
        Me.GroupBox1.TabStop = False
        '
        'Label1
        '
        Me.Label1.AutoSize = True
        Me.Label1.Location = New System.Drawing.Point(6, 16)
        Me.Label1.Name = "Label1"
        Me.Label1.Size = New System.Drawing.Size(59, 12)
        Me.Label1.TabIndex = 15
        Me.Label1.Text = "语句类型:"
        '
        'btnOK
        '
        Me.btnOK.Location = New System.Drawing.Point(218, 124)
        Me.btnOK.Name = "btnOK"
        Me.btnOK.Size = New System.Drawing.Size(57, 33)
        Me.btnOK.TabIndex = 16
        Me.btnOK.Text = "设置"
        Me.btnOK.UseVisualStyleBackColor = True
        '
        'chkPrintSQL
        '
        Me.chkPrintSQL.AutoSize = True
        Me.chkPrintSQL.Location = New System.Drawing.Point(10, 10)
        Me.chkPrintSQL.Name = "chkPrintSQL"
        Me.chkPrintSQL.Size = New System.Drawing.Size(114, 16)
        Me.chkPrintSQL.TabIndex = 17
        Me.chkPrintSQL.Text = "输出打印SQL语句"
        Me.chkPrintSQL.UseVisualStyleBackColor = True
        '
        'Label4
        '
        Me.Label4.AutoSize = True
        Me.Label4.Location = New System.Drawing.Point(11, 114)
        Me.Label4.Name = "Label4"
        Me.Label4.Size = New System.Drawing.Size(71, 12)
        Me.Label4.TabIndex = 28
        Me.Label4.Text = "执行时间>=:"
        '
        'txtMsExec
        '
        Me.txtMsExec.Enabled = False
        Me.txtMsExec.ImeMode = System.Windows.Forms.ImeMode.Disable
        Me.txtMsExec.Location = New System.Drawing.Point(81, 110)
        Me.txtMsExec.Name = "txtMsExec"
        Me.txtMsExec.Size = New System.Drawing.Size(79, 21)
        Me.txtMsExec.TabIndex = 29
        Me.txtMsExec.Text = "0"
        '
        'Label5
        '
        Me.Label5.AutoSize = True
        Me.Label5.Location = New System.Drawing.Point(162, 114)
        Me.Label5.Name = "Label5"
        Me.Label5.Size = New System.Drawing.Size(29, 12)
        Me.Label5.TabIndex = 30
        Me.Label5.Text = "(ms)"
        '
        'GroupBox2
        '
        Me.GroupBox2.Controls.Add(Me.chkStaExec3)
        Me.GroupBox2.Controls.Add(Me.chkStaExec2)
        Me.GroupBox2.Controls.Add(Me.chkStaExec1)
        Me.GroupBox2.Controls.Add(Me.Label2)
        Me.GroupBox2.Enabled = False
        Me.GroupBox2.Location = New System.Drawing.Point(5, 67)
        Me.GroupBox2.Name = "GroupBox2"
        Me.GroupBox2.Size = New System.Drawing.Size(271, 35)
        Me.GroupBox2.TabIndex = 18
        Me.GroupBox2.TabStop = False
        '
        'chkStaExec3
        '
        Me.chkStaExec3.AutoSize = True
        Me.chkStaExec3.Location = New System.Drawing.Point(178, 13)
        Me.chkStaExec3.Name = "chkStaExec3"
        Me.chkStaExec3.Size = New System.Drawing.Size(47, 16)
        Me.chkStaExec3.TabIndex = 27
        Me.chkStaExec3.Text = "失败"
        Me.chkStaExec3.UseVisualStyleBackColor = True
        '
        'chkStaExec2
        '
        Me.chkStaExec2.AutoSize = True
        Me.chkStaExec2.Location = New System.Drawing.Point(123, 13)
        Me.chkStaExec2.Name = "chkStaExec2"
        Me.chkStaExec2.Size = New System.Drawing.Size(47, 16)
        Me.chkStaExec2.TabIndex = 26
        Me.chkStaExec2.Text = "成功"
        Me.chkStaExec2.UseVisualStyleBackColor = True
        '
        'chkStaExec1
        '
        Me.chkStaExec1.AutoSize = True
        Me.chkStaExec1.Checked = True
        Me.chkStaExec1.Location = New System.Drawing.Point(72, 13)
        Me.chkStaExec1.Name = "chkStaExec1"
        Me.chkStaExec1.Size = New System.Drawing.Size(47, 16)
        Me.chkStaExec1.TabIndex = 25
        Me.chkStaExec1.TabStop = True
        Me.chkStaExec1.Text = "不限"
        Me.chkStaExec1.UseVisualStyleBackColor = True
        '
        'Label2
        '
        Me.Label2.AutoSize = True
        Me.Label2.Location = New System.Drawing.Point(6, 15)
        Me.Label2.Name = "Label2"
        Me.Label2.Size = New System.Drawing.Size(59, 12)
        Me.Label2.TabIndex = 24
        Me.Label2.Text = "执行状态:"
        '
        'Label3
        '
        Me.Label3.AutoSize = True
        Me.Label3.Location = New System.Drawing.Point(12, 167)
        Me.Label3.Name = "Label3"
        Me.Label3.Size = New System.Drawing.Size(155, 12)
        Me.Label3.TabIndex = 31
        Me.Label3.Text = "SQL过滤(支持正则表达式)："
        '
        'txtFilter
        '
        Me.txtFilter.Enabled = False
        Me.txtFilter.Location = New System.Drawing.Point(14, 186)
        Me.txtFilter.Name = "txtFilter"
        Me.txtFilter.Size = New System.Drawing.Size(261, 21)
        Me.txtFilter.TabIndex = 32
        '
        'Label6
        '
        Me.Label6.AutoSize = True
        Me.Label6.Location = New System.Drawing.Point(12, 144)
        Me.Label6.Name = "Label6"
        Me.Label6.Size = New System.Drawing.Size(65, 12)
        Me.Label6.TabIndex = 33
        Me.Label6.Text = "客户端IP："
        '
        'txtIP
        '
        Me.txtIP.Enabled = False
        Me.txtIP.Location = New System.Drawing.Point(81, 139)
        Me.txtIP.Name = "txtIP"
        Me.txtIP.Size = New System.Drawing.Size(110, 21)
        Me.txtIP.TabIndex = 34
        '
        'chkType1
        '
        Me.chkType1.AutoSize = True
        Me.chkType1.Checked = True
        Me.chkType1.CheckState = System.Windows.Forms.CheckState.Checked
        Me.chkType1.Location = New System.Drawing.Point(72, 15)
        Me.chkType1.Name = "chkType1"
        Me.chkType1.Size = New System.Drawing.Size(48, 16)
        Me.chkType1.TabIndex = 20
        Me.chkType1.Text = "查询"
        Me.chkType1.UseVisualStyleBackColor = True
        '
        'chkType3
        '
        Me.chkType3.AutoSize = True
        Me.chkType3.Checked = True
        Me.chkType3.CheckState = System.Windows.Forms.CheckState.Checked
        Me.chkType3.Location = New System.Drawing.Point(180, 15)
        Me.chkType3.Name = "chkType3"
        Me.chkType3.Size = New System.Drawing.Size(48, 16)
        Me.chkType3.TabIndex = 21
        Me.chkType3.Text = "事务"
        Me.chkType3.UseVisualStyleBackColor = True
        '
        'chkType2
        '
        Me.chkType2.AutoSize = True
        Me.chkType2.Checked = True
        Me.chkType2.CheckState = System.Windows.Forms.CheckState.Checked
        Me.chkType2.Location = New System.Drawing.Point(126, 15)
        Me.chkType2.Name = "chkType2"
        Me.chkType2.Size = New System.Drawing.Size(48, 16)
        Me.chkType2.TabIndex = 22
        Me.chkType2.Text = "写入"
        Me.chkType2.UseVisualStyleBackColor = True
        '
        'frmPrintSQL
        '
        Me.AutoScaleDimensions = New System.Drawing.SizeF(6.0!, 12.0!)
        Me.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font
        Me.ClientSize = New System.Drawing.Size(288, 214)
        Me.Controls.Add(Me.txtIP)
        Me.Controls.Add(Me.Label6)
        Me.Controls.Add(Me.txtFilter)
        Me.Controls.Add(Me.Label3)
        Me.Controls.Add(Me.Label5)
        Me.Controls.Add(Me.txtMsExec)
        Me.Controls.Add(Me.GroupBox2)
        Me.Controls.Add(Me.Label4)
        Me.Controls.Add(Me.chkPrintSQL)
        Me.Controls.Add(Me.btnOK)
        Me.Controls.Add(Me.GroupBox1)
        Me.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedSingle
        Me.MaximizeBox = False
        Me.MinimizeBox = False
        Me.Name = "frmPrintSQL"
        Me.ShowInTaskbar = False
        Me.StartPosition = System.Windows.Forms.FormStartPosition.CenterParent
        Me.Text = "SQL打印输出控制参数"
        Me.TopMost = True
        Me.GroupBox1.ResumeLayout(False)
        Me.GroupBox1.PerformLayout()
        Me.GroupBox2.ResumeLayout(False)
        Me.GroupBox2.PerformLayout()
        Me.ResumeLayout(False)
        Me.PerformLayout()

    End Sub
    Friend WithEvents GroupBox1 As System.Windows.Forms.GroupBox
    Friend WithEvents Label1 As System.Windows.Forms.Label
    Friend WithEvents btnOK As System.Windows.Forms.Button
    Friend WithEvents chkPrintSQL As System.Windows.Forms.CheckBox
    Friend WithEvents Label5 As System.Windows.Forms.Label
    Friend WithEvents txtMsExec As System.Windows.Forms.TextBox
    Friend WithEvents Label4 As System.Windows.Forms.Label
    Friend WithEvents GroupBox2 As System.Windows.Forms.GroupBox
    Friend WithEvents chkStaExec3 As System.Windows.Forms.RadioButton
    Friend WithEvents chkStaExec2 As System.Windows.Forms.RadioButton
    Friend WithEvents chkStaExec1 As System.Windows.Forms.RadioButton
    Friend WithEvents Label2 As System.Windows.Forms.Label
    Friend WithEvents Label3 As System.Windows.Forms.Label
    Friend WithEvents txtFilter As System.Windows.Forms.TextBox
    Friend WithEvents Label6 As System.Windows.Forms.Label
    Friend WithEvents txtIP As System.Windows.Forms.TextBox
    Friend WithEvents chkType2 As System.Windows.Forms.CheckBox
    Friend WithEvents chkType3 As System.Windows.Forms.CheckBox
    Friend WithEvents chkType1 As System.Windows.Forms.CheckBox
End Class
