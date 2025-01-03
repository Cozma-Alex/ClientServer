import random
import sys

def generate_competitors(num_countries, competitors_per_country):
    for country in range(1, num_countries + 1):
        with open(f'competitors_{country}.txt', 'w') as f:
            for i in range(1, competitors_per_country + 1):
                score = random.randint(60, 100)
                f.write(f"{i} {score}\n")

if __name__ == "__main__":
    num_countries = int(sys.argv[1]) if len(sys.argv) > 1 else 5
    competitors_per_country = int(sys.argv[2]) if len(sys.argv) > 2 else 100
    generate_competitors(num_countries, competitors_per_country)