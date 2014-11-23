rm -rf slurm_varios/acct/*
rm -rf slurm_varios/log/*
rm -rf slurm_varios/var/state/*

mysql -u root --password=slurm < ./mysql-scripts/delete_slurm_tables_info
