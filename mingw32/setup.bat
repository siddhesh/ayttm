IF "%2" == "" goto single

echo Multiple
echo default_arrive_snd=%1 %2\sounds\BuddyArrive.au >> "%1 %2\etc\ayttmrc"
echo default_leave_snd=%1 %2\sounds\BuddyLeave.au >> "%1 %2\etc\ayttmrc"
echo default_send_snd=%1 %2\sounds\Send.au >> "%1 %2\etc\ayttmrc"
echo default_recv_snd=%1 %2\sounds\Receive.au >> "%1 %2\etc\ayttmrc"
echo default_module_path=%1 %2\modules >> "%1 %2\etc\ayttmrc"
goto end

:single
echo Single
echo default_arrive_snd=%1\sounds\BuddyArrive.au >> %1\etc\ayttmrc
echo default_leave_snd=%1\sounds\BuddyLeave.au >> %1\etc\ayttmrc
echo default_send_snd=%1\sounds\Send.au >> %1\etc\ayttmrc
echo default_recv_snd=%1\sounds\Receive.au >> %1\etc\ayttmrc
echo default_module_path=%1\modules >> %1\etc\ayttmrc

:end
