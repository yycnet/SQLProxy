Module mdl_sqlproxy
    '最大数据库集群个数,此数值要和frmMain中定义的panServerX的个数一致
    Public Const MAX_DBCOUNTS As Integer = 6

    Public Structure TDBServer
        Dim host As String
        Dim port As Integer
        Dim valid As Integer
        Dim dbuser As String  '允许后台集群服务设置不同的访问帐号和密码
        Dim dbpswd As String
    End Structure

    Public g_dbname As String '数据库名
    Public g_dbuser As String '数据库帐号密码
    Public g_dbpswd As String
    Public g_dbservers As New ArrayList

    'szDBInfo - 配置的集群数据库信息，格式 <host:port:user:pswd>,<host:port:user:pswd>,...
    Public Sub InitDBServers(ByRef szDBinfo As String)
        g_dbservers.Clear()
        AddDBServer(szDBinfo)
    End Sub
    Public Sub AddDBServer(ByRef szDBinfo As String)
        Dim arrDBsvr() As String = szDBinfo.Split(",")
        For idx As Integer = 0 To arrDBsvr.Length - 1 Step 1
            If arrDBsvr(idx) <> "" Then
                Dim hostinfo() As String = arrDBsvr(idx).Split(":")
                If hostinfo.Length > 0 Then
                    Dim dbsvr As TDBServer
                    dbsvr.port = 1433
                    dbsvr.host = hostinfo(0)
                    If hostinfo.Length > 1 Then
                        dbsvr.port = Convert.ToInt32(hostinfo(1))
                        If dbsvr.port <= 0 Then dbsvr.port = 1433
                    End If
                    If hostinfo.Length > 2 Then dbsvr.dbuser = hostinfo(2)
                    If hostinfo.Length > 3 Then dbsvr.dbpswd = hostinfo(3)
                    g_dbservers.Add(dbsvr)
                End If
            End If
            If g_dbservers.Count >= MAX_DBCOUNTS Then Exit For
        Next
    End Sub

    Public Function FindControlByName(ByVal container As Control, ByVal name As String)
        For Each mycontrol In container.Controls '遍历所有控件
            If (mycontrol.Name = name) Then
                Return mycontrol
            ElseIf mycontrol.Controls.Count > 0 Then
                '父容器，递归遍历
                Dim retControl As Control = Nothing
                retControl = FindControlByName(mycontrol, name)
                If retControl IsNot Nothing Then Return retControl
            End If
        Next
        Return Nothing
    End Function

    Public Const LOGLEVEL_NONE As Integer = -1
    Public Const LOGLEVEL_DEBUG As Integer = 0
    Public Const LOGLEVEL_WARN As Integer = 1
    Public Const LOGLEVEL_INFO As Integer = 2
    Public Const LOGLEVEL_ERROR As Integer = 3

    Declare Sub SetLogLevel Lib "SQLProxy" (ByVal hwnd As System.IntPtr, ByVal LogLevel As Integer)
    Declare Function DBTest Lib "SQLProxy" (ByVal szHost As String, ByVal iport As Integer, ByVal szUser As String, ByVal szPswd As String, ByVal dbname As String) As Integer

    Declare Sub SetParameters Lib "SQLProxy" (ByVal SetID As Integer, ByVal szParam As String)
    Declare Function GetDBStatus Lib "SQLProxy" (ByVal idx As Integer, ByVal proid As Integer) As Integer
    Declare Function StartProxy Lib "SQLProxy" (ByVal iport As Integer) As Boolean
    Declare Sub StopProxy Lib "SQLProxy" ()
    Declare Function ExecFileData Lib "SQLProxy" Alias "ProxyCall" (ByVal callID As Integer, ByVal filename As String) As Integer

End Module
