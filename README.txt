This is a project written in C with the objective of simulating an online multiplayer game.


To get the game running you need to run the "build.bat" file in the "Bomberman_Multiplayer" directory.
After that you should be able to see an executable file in the "bin" subfolder.


In order to connect with a friend it is necessary for one of to do the server's part 
while the other does the client's part.



The one doing the server part needs to follow these steps:

You have to create an UDP rule on your router for a specific port, we defined 8888 as 
the default port but it works with others too as long as you make a rule for them.
Then you need the public IP of your computer, to do that you can visit https://whatismyipaddress.com/ .
Once you have your IP address, give it to your friend alongside a port number, like 8888 for example.
Then open the command prompt from the src folder and write the following string 
with the following syntax: 
"server_python.py [IP] [Port]" with ip being your public IP and the port being a number one you decided. 


Instructions for clients:
Once you receive the public IP of your friend, open the command prompt from the 
bin folder and write the following string with the following syntax:
"bomberman_client.exe [IP] [Port]" with ip being your friend's public IP and the port being 
the same port of the server.

Keep in mind that the person doing the server part can also play the game following the client steps!

This project was created with the collaboration of Francesco Marotta, who helped me testing the network features.



