
Namespace HashTEA
    Public Class hashtea

        Private Const delta As UInteger = &H9E3779B9L    'tea算法的delta值
        Private Plain(7) As Byte                         '指向当前的明文块 
        Private prePlain(7) As Byte                      '指向前面一个明文块 
        Private out() As Byte
        Private Crypt As UInteger
        Private preCrypt As UInteger    '当前加密的密文位置和上一次加密的密文块位置，他们相差8 
        Private Pos As Long             '当前处理的加密解密块的位置 
        Private padding As Long         '填充数 
        Private Key(15) As Byte         '密钥 
        Private Header As Boolean       '用于加密时，表示当前是否是第一个8字节块，因为加密算法 
        '是反馈的，但是最开始的8个字节没有反馈可用，所有需要标明这种情况 
        Private contextStart As Long
        Public Function UnHashTEA(ByVal BinFrom As Byte(), ByVal BinTKey As Byte(), _
                                  ByVal offset As Integer, ByVal Is16Rounds As Boolean) As Byte()
            Crypt = 0
            preCrypt = 0
            Key = BinTKey
            Dim count As Integer = 0
            Dim m(offset + 7) As Byte
            Dim intlen As Integer = BinFrom.Length
            If intlen < 16 Or (intlen Mod 8 <> 0) Then ThrowMsg("Len No Enuf")
            prePlain = Decipher(BinFrom, Key, True)
            Pos = prePlain(0) And &H7
            count = intlen - Pos - 10
            If count < 0 Then ThrowMsg("Count No Enuf")
            For i = offset To m.Length - 1
                m(i) = 0
            Next
            ReDim out(count - 1)
            preCrypt = 0
            Crypt = 8
            contextStart = 8
            Pos += 1

            padding = 1
            While padding <= 2

                If Pos < 8 Then
                    Pos += 1
                    padding += 1

                End If
                If Pos = 8 Then
                    m = BinFrom
                    If Not (Decrypt8Bytes(BinFrom, offset, intlen)) Then
                        ThrowMsg("Decrypt8Bytes() failed.")
                    End If
                End If
            End While

            Dim i2 = 0
            While count <> 0
                If Pos < 8 Then
                    out(i2) = CByte(m(offset + preCrypt + Pos) Xor prePlain(Pos))
                    i2 += 1
                    count -= 1
                    Pos += 1
                End If

                If Pos = 8 Then
                    m = BinFrom
                    preCrypt = Crypt - 8
                    Decrypt8Bytes(BinFrom, offset, intlen)
                End If

            End While

            For i = 1 To 7
                If Pos < 8 Then
                    If (m(offset + preCrypt + Pos) Xor prePlain(Pos)) <> 0 Then
                        ThrowMsg("tail is not filled correct.")
                    End If
                    Pos += 1
                    If Pos = 8 Then
                        m = BinFrom
                        If Not (Decrypt8Bytes(BinFrom, offset, intlen)) Then
                            ThrowMsg("Decrypt8Bytes() failed.")
                        End If

                    End If
                End If
            Next

            Return out

        End Function

        Private Function Decrypt8Bytes(ByVal input() As Byte, ByVal offset As Integer, _
                                      ByVal intlen As Integer) As Boolean

            For i = 0 To 7
                If contextStart + i >= intlen Then
                    Return True
                End If
                prePlain(i) = prePlain(i) Xor input(offset + Crypt + i)

            Next

            prePlain = Decipher(prePlain, Key, True)
            If prePlain Is Nothing Then
                Return False

            End If
            contextStart += 8
            Crypt += 8
            Pos = 0

            Return True
        End Function

        Private Function Decipher(ByVal BinInput() As Byte, _
          ByVal Binkey() As Byte, ByVal Is16Rounds As Boolean) As Byte()
            '标准tea解密过程,参数ltype 为1时表示16轮迭代(qq使用的就是16轮迭代),否则为32轮迭代

            Dim sum As Long = &HE3779B90L
            Dim rounds As Integer

            Dim y As Long = GetUInt(BinInput, 0, 4)
            Dim z As Long = GetUInt(BinInput, 4, 4)
            Dim a As Long = GetUInt(Key, 0, 4)
            Dim b As Long = GetUInt(Key, 4, 4)
            Dim c As Long = GetUInt(Key, 8, 4)
            Dim d As Long = GetUInt(Key, 12, 4)

            If Is16Rounds Then
                rounds = 16
            Else
                rounds = 32
            End If
            Dim Test As Long = 0
            For i = 1 To rounds

                Test = ((y << 4) + c) Xor (y + sum) Xor ((y >> 5) + d)
                z -= Test
                z = z And &HFFFFFFFFL

                Test = ((z << 4) + a) Xor (z + sum) Xor ((z >> 5) + b)
                y -= Test
                y = y And &HFFFFFFFFL

                sum -= delta
                sum = sum And &HFFFFFFFFL

            Next

            Return ToBytes(y, z)
        End Function

        Public Function HashTEA(ByVal BinFrom As Byte(), ByVal BinTKey As Byte(), _
            ByVal offset As Integer, ByVal Is16Rounds As Boolean) As Byte()

            Header = True
            Key = BinTKey
            Pos = 1
            padding = 0
            Crypt = 0
            preCrypt = 0
            Dim intlen As Integer = BinFrom.Length
            Dim xRnd As New Random
            Pos = (intlen + 10) Mod 8

            If Pos <> 0 Then Pos = 8 - Pos
            ReDim out(intlen + Pos + 9)


            Plain(0) = CByte((xRnd.Next And &HF8) Or Pos)

            For i = 1 To Pos
                Plain(i) = CByte(xRnd.Next And &HFF)
            Next

            For i = 0 To 7
                prePlain(i) = CByte(&H0)
            Next

            Pos += 1
            padding = 1
            Do While padding < 3
                If Pos < 8 Then
                    Plain(Pos) = CByte(xRnd.Next And &HFF)
                    Pos += 1
                    padding += 1

                Else
                    Encrypt8Bytes(Is16Rounds)
                End If
            Loop

            Dim i2 = offset
            While intlen > 0
                If Pos < 8 Then

                    Plain(Pos) = BinFrom(i2)
                    Pos += 1
                    intlen -= 1

                    i2 += 1
                Else
                    Encrypt8Bytes(Is16Rounds)

                End If
            End While

            padding = 1
            While padding < 8
                If Pos < 8 Then

                    Plain(Pos) = &H0
                    padding += 1
                    Pos += 1
                End If

                If Pos = 8 Then
                    Encrypt8Bytes(Is16Rounds)
                End If
            End While

            Return out
        End Function

        Private Sub Encrypt8Bytes(ByVal Is16Rounds As Boolean)
            Dim Crypted() As Byte
            Pos = 0
            For i = 0 To 7
                If Header Then
                    Plain(i) = Plain(i) Xor prePlain(0)
                Else
                    Plain(i) = Plain(i) Xor out(preCrypt + i)
                End If
            Next
            Crypted = Encipher(Plain, Key, Is16Rounds)
            Array.Copy(Crypted, 0, out, Crypt, 8)
            For i = 0 To 7
                out(Crypt + i) = out(Crypt + i) Xor prePlain(i)

            Next
            Array.Copy(Plain, 0, prePlain, 0, 8)
            preCrypt = Crypt
            Crypt += 8
            Pos = 0
            Header = False

        End Sub

        Private Function Encipher(ByVal BinInput() As Byte, ByVal k() As Byte, ByVal Is16Rounds As Boolean)
            '标准的tea加密过程,参数 Is16Rounds 为True时表示16轮迭代(qq使用的就是16轮迭代),否则为32轮迭代

            Dim sum As ULong

            Dim rounds As Integer

            Dim y As ULong = GetUInt(BinInput, 0, 4)
            Dim z As ULong = GetUInt(BinInput, 4, 4)
            Dim a As ULong = GetUInt(Key, 0, 4)
            Dim b As ULong = GetUInt(Key, 4, 4)
            Dim c As ULong = GetUInt(Key, 8, 4)
            Dim d As ULong = GetUInt(Key, 12, 4)

            If Is16Rounds Then
                rounds = 16
            Else
                rounds = 32
            End If

            For i = 1 To rounds
                sum = sum And &HFFFFFFFFL
                sum += delta
                z = z And &HFFFFFFFFL
                y += ((z << 4) + a) Xor (z + sum) Xor ((z >> 5) + b)
                y = y And &HFFFFFFFFL
                z += ((y << 4) + c) Xor (y + sum) Xor ((y >> 5) + d)

            Next
            Return ToBytes(y, z)
        End Function

        Public Function GetUInt(ByVal input As Byte(), ByVal ioffset As Integer, ByVal intlen As Integer) As UInteger

            Dim ret As UInteger = 0
            Dim lend As Integer = IIf((intlen > 4), (ioffset + 4), (ioffset + intlen))
            For i = ioffset To lend - 1
                ret <<= 8
                ret = ret Or input(i)
            Next
            Return ret
        End Function

        Public Function ToBytes(ByVal a As ULong, ByVal b As ULong) As Byte()

            Dim bytes(7) As Byte

            bytes(0) = CByte((a >> 24) And &HFF)
            bytes(1) = CByte((a >> 16) And &HFF)
            bytes(2) = CByte((a >> 8) And &HFF)
            bytes(3) = CByte((a) And &HFF)
            bytes(4) = CByte((b >> 24) And &HFF)
            bytes(5) = CByte((b >> 16) And &HFF)
            bytes(6) = CByte((b >> 8) And &HFF)
            bytes(7) = CByte((b) And &HFF)
            Return bytes
        End Function

        Private Sub ThrowMsg(ByVal TMsg As String)
            Dim Trmsg As New MyException(TMsg)
            Throw Trmsg
        End Sub

    End Class

    Public Class MyException
        Inherits System.ApplicationException

        Public Sub New(ByVal StrMsg As String)
            MyBase.New(StrMsg)

        End Sub

    End Class
End Namespace
