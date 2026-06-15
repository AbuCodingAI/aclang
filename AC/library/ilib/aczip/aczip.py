class aczip:
    #This program uses a 4 bit protocol to zip up ac file archives, use ilib aczip allows one to fully archive a Project
    #This uses a technique that spreads the zipping process over multiple CPU cores and zips up file systems in paralell, it marks each file with a file tag, and each folder with a folder tag, then it goes inside each folder, and gives a tag to every file and folder, which is preceded by a prefix of the folder
    #i.e. I have a file system, assets/ src/ public/ README.md
    # README.md gets tagged 0x01
    #assets/ gets tagged 1x01
    #src/ gets tagged 1x02
    #public/ gets tagged 1x03
    #assets/ has enemy.png, player.png, obstacle.jpg
    #enemy.png is 1x01.0x01
    #player.png is 1x01.0x02 and so on
def aczip_hdd_zip:
    #NIcer towards an HDD
def aczip_sata_zip:
    #Nicer towards a SATA