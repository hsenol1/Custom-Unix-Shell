import sys
from tradingview_ta import TA_Handler, Interval, Exchange


print(sys.argv[0]);
symbol1 = sys.argv[1]
screener1 = sys.argv[2]
exchange1 = sys.argv[3]

'''
symbol1 = "BTCUSDT";
screener1 = "crypto";
exchange1 = "BINANCE";

''' 
coin = TA_Handler(
	symbol = symbol1,
	screener = screener1,
	exchange = exchange1,
	interval = Interval.INTERVAL_1_HOUR

)


data2 = coin.get_analysis().moving_averages
data3 = coin.get_analysis().oscillators
data4 = coin.get_analysis().summary

print("Moving averages is listed\n")
print(data2)
print("\n")

print("Oscillators are listed\n")
print(data3)
print("\n")

print("Here is the summary, constructed by indicators.\n")
print(data4)
print("\n")

print("In the end, the general assumption is {fname}".format(fname  = data4.get('RECOMMENDATION')))



