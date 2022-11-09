# Copy paste the following bash commands to the ~/.bashrc file on Operator PC

cam4k_dev=$(v4l2-ctl --list-devices | awk '/Cam Link 4K/ {getline;print}' | sed 's|^[ \t]*||')
echo Cam 4k device path: $cam4k_dev

spatial_card=$(arecord -l | awk '/C4K/ {print}' | grep -oP '(?<=card )[0-9]+')
echo Spatial mic capture card: $spatial_card

spatial_dev=$(arecord -l | awk '/C4K/ {print}' | grep -oP '(?<=device )[0-9]+')
echo Spatial mic capture device: $spatial_dev

# Modify main camera config file
sed -i "s|/dev/video[0-9]|$cam4k_dev|" ../configs/blackmagic.config