# Modified-Unix-V6-File-System

Implementation of a modified Unix Version 6 file system. Developed a command line interface that allows a Unix user to access the file system of a foreign operating system which is a modified version of Unix Version 6.

**Complilation instructions:**  
```gcc fsaccess.c -ofs```

**Run:**    
```./fs```

**User Input / Available commands:**    

```initfs filename numberOfBlocks numberOfInodes``` - To initialize the file system ex: initfs v6filesystem 8000 300   
```cpin externalFile V6File``` - To copy contents from an external file to a new file in the V6 file system ex: cpin test.txt fstest.txt  
```cpout V6File externalFile``` - To copy contents from a file in the V6 file system to an external file ex: cpout fstest.txt test2.txt   
```mkdir directoryName``` - To create a directory ex: mkdir /user  
```cd directoryName``` - To change directory ex: cd /user or cd ..  
```rm FileName``` - To remove a file ex: rm fstest.txt   
```pwd``` - To print current working directory  
```q``` - To exit file system  

**Additional Notes:**  
File size is limited to 14 characters  
fsaccess.c run using Putty and csgrads1@utdallas.edu server
