echo "____________WELCOME TO XXFTPACKET____________"
echo "__________By A. Roos & S. Rosetter___________"
echo ""
echo "Would you like to be sender or receiver?"
echo "Enter 1 for sender or 2 for receiver"
read selection

if [ $selection -eq 1 ]
then
	echo "You have chosen to be the sender"
	./TCP_client
elif [ $selection -eq 2 ]
then
	echo "You have chosen to be the receiver"
	./TCP_server
else
	echo "You made an incorrect choice"
	./select_side.sh
fi
