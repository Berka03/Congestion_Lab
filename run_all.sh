#!/bin/bash
# Simple script to run congestion simulation with different queue disciplines

# Base command
NS3_CMD="../.././ns3 run scratch/congestion_simulation/Sim3 --"

# List of queue algorithms
ALGORITHMS=(
  "ns3::PfifoFastQueueDisc"
  "ns3::RedQueueDisc"
  "ns3::CoDelQueueDisc"
  "ns3::FqCoDelQueueDisc"
  "ns3::PieQueueDisc"
)

rm -r results/
# Create results directory
mkdir -p results

echo "============================================"
echo "Running simulations with different queue disciplines"
echo "============================================"

# Run each simulation
for ALG in "${ALGORITHMS[@]}"; do
  ALG_NAME=$(echo "$ALG" | awk -F'::' '{print $2}')
  echo "Running: $ALG_NAME"
  
  $NS3_CMD --queueDiscType="$ALG" > "results/${ALG_NAME}.txt"
  
  echo "Finished: $ALG_NAME"
  echo "---"
done

echo "All simulations completed!"
echo "Results saved in results/"