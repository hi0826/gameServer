#include "includes.h"
#include "Network.h"

Network gNetworkManager;
int main()
{
	gNetworkManager.Initialize();

	gNetworkManager.Run();
}