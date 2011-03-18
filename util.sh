# Report fix

#archive the code
cd report
python shaWriter.py
make
make veryclean
cd ..
zip -r report.zip report

# Code fix

zip -r source_code.zip src
