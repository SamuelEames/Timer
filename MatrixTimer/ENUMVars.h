// Extra variables to go with timer because apparently Arduino doesn't accept enum types in .ino files


// Game State 		0			1		2			3			4			5			6			7			8				9				10		
enum timerStates {ST_NULL, ST_SW_READY, ST_SW_RUN, ST_SW_STOP, ST_RT_LOBBY, ST_RT_READY, ST_RT_GO, ST_RT_STOP};

