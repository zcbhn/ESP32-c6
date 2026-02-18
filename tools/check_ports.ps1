Get-PnpDevice | Where-Object { $_.FriendlyName -match 'USB|JTAG|Serial|CP210|CH340|FTDI|ESP' } | Select-Object FriendlyName,Status,InstanceId | Format-List
