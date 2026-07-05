import pandas as pd
from sklearn.linear_model import LinearRegression

# 1. Load the dataset
data = pd.read_csv("train.csv")

# 2. Extract features (X) and target label (y)
X = data[['IR']] # Optical metric from MAX30102
y = data['Ref'] # Clinical finger-prick value

# 3. Train the Linear Regression Model
model = LinearRegression()
model.fit(X, y)

# 4. Extract Machine Learning parameters
slope = model.coef_[0]
intercept = model.intercept_

print("=== ML MODEL TRAINING COMPLETE ===")
print(f"Calculated Slope (m):     {slope:.1f}")       
print(f"Calculated Intercept (c): {intercept:.1f}")   
print("\nDeployment Formula:")
print(f"Glucose = (Ratio * {slope:.1f}) + {intercept:.1f}")
