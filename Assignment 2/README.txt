Instructions to compile and run the code

1. First of all open terminal, then go to location of assignment
2. Now to compile network.c using command "gcc network.c -o network"
3. Now compile client.c using gcc client.c -o client
Note:- numbers to be checked are hard coded as:
    1)4083902387: Access Failed i.e., unpaid
    2)4083992399; Access Failed i.e., technology mismatch
    3)4083002300; Access Failed i.e., does not exist in database
    4)4083902378; Access Success i.e., successful access
4. Now first run server using "./server" and then client using "./client"

Note- For server unresponsive test case, run "./client" without running the "./network" first.