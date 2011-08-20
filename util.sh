# Report fix

#archive the code
cd report
make
make veryclean
cd ..
zip -r report.zip report

# Code fix

zip -r source_code.zip src
