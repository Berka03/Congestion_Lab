# Congestion_Lab for DTA 400 simulation project

To run the simulation put it in the scratch folder in the ns3 directory. 

Then ./ns3 build 

When it is used the congestion lab folder is in the directory run the sim3.cc with the ./run_all.sh

It will create a results folder with text documents with the different results over the performance for the queue algorithms. 


If you want to run the algorithms alone one by one 

../.././ns3 run "scratch/congestion_simulation/Sim3 -- --queueDiscType=ns3::PfifoFastQueueDisc --nSenders=8"

../.././ns3 run "scratch/congestion_simulation/Sim3 -- --queueDiscType=ns3::CoDelQueueDisc --nSenders=8"

../.././ns3 run "scratch/congestion_simulation/Sim3 -- --queueDiscType=ns3::FqCoDelQueueDisc --nSenders=8"

../.././ns3 run "scratch/congestion_simulation/Sim3 -- --queueDiscType=ns3::RedQueueDisc --nSenders=8"

../.././ns3 run "scratch/congestion_simulation/Sim3 -- --queueDiscType=ns3::PieQueueDisc --nSenders=8"
