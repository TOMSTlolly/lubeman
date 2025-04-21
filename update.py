import paramiko

clients = [
    {"host": "10.0.0.144"}
    ]

login = "pi"
passwd="raspberry"

for client in clients:
    print ("client ",client["host"])   
    ssh = paramiko.SSHClient()
    ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    ssh.connect(client["host"], username=login, password=passwd)

    # Start SFTP session
    sftp = ssh.open_sftp()
    ## pretahni archiv
    local="/home/krata/lubeman_x86.tgz"
    remote="/home/pi/lubemanx.tgz"
    sftp.put(local,remote)
    ## pretahni skript, ktery zalohuje a rozbaly novy sw
    local ="/home/krata/lubeman/back.sh"
    remote ="/home/pi/back.sh"
    sftp.put(local,remote)
    sftp.close()

    # execute commands
    #command = "bash ./back.sh"
    #stdin, stdout, stderr = ssh.exec_command(command)
    #print(stdout.read().decode())
    #ssh.close()

