#include "common.h"
#include "communication.h"
#include "config.h"
#include "houses.h"
#include "info.h"
#include "magic.h"
#include "map.h"
#include "moveuse.h"
#include "objects.h"
#include "operate.h"
#include "query.h"
#include "reader.h"
#include "writer.h"

#include <fstream>
#include <fcntl.h>
#include <unistd.h>
#include <ucontext.h>
#include <signal.h>
#include <sys/stat.h>

static bool BeADaemon = false;
static bool Reboot = false;
static bool SaveMapOn = false;

static timer_t BeatTimer;
static int SigAlarmCounter = 0;
static int SigUsr1Counter = 0;

static sighandler_t SigHandler(int SigNr, sighandler_t Handler) {
	struct sigaction Action;
	struct sigaction OldAction;

	Action.sa_handler = Handler;

	// TODO(fusion): I feel we should probably use `sigfillset` specially for
	// signals that share the same handler.
	sigemptyset(&Action.sa_mask);

	if (SigNr == SIGALRM) {
		// SA_INTERRUPT is a glibc extension (absent on musl). The default
		// behavior without SA_RESTART is to not restart syscalls, which
		// is exactly what SA_INTERRUPT requests — so 0 is equivalent.
		Action.sa_flags = 0;
	} else {
		Action.sa_flags = SA_RESTART;
	}

	if (sigaction(SigNr, &Action, &OldAction) == 0) {
		return OldAction.sa_handler;
	} else {
		return SIG_ERR;
	}
}

static void SigBlock(int SigNr) {
	sigset_t Set;
	sigemptyset(&Set);
	sigaddset(&Set, SigNr);
	if (sigprocmask(SIG_BLOCK, &Set, NULL) == -1) {
		error("SigBlock: Failed to block signal %d (%s): (%d) %s\n", SigNr, strsignal(SigNr), errno, strerror(errno));
	}
}

static void SigWaitAny(void) {
	sigset_t Set;
	sigemptyset(&Set);
	sigsuspend(&Set);
}

static void SigHupHandler(int signr) {
	// no-op (?)
}

static void SigAbortHandler(int signr) {
	print(1, "SigAbortHandler: shutting down writer thread.\n");
	abort_writer();
}

static void DefaultHandler(int signr) {
	print(1, "DefaultHandler: Shutting down game server (SigNr. %d: %s).\n", signr, strsignal(signr));

	SigHandler(SIGINT, SIG_IGN);
	SigHandler(SIGQUIT, SIG_IGN);
	SigHandler(SIGTERM, SIG_IGN);
	SigHandler(SIGXCPU, SIG_IGN);
	SigHandler(SIGXFSZ, SIG_IGN);
	SigHandler(SIGPWR, SIG_IGN);

	SaveMapOn = (signr == SIGQUIT) || (signr == SIGTERM) || (signr == SIGPWR);
	if (signr == SIGTERM) {
		int Hour, Minute;
		GetRealTime(&Hour, &Minute);
		RebootTime = (Hour * 60 + Minute + 6) % 1440;
		CloseGame();
	} else {
		EndGame();
	}

	Reboot = false;
}

#if 0
// TODO(fusion): This function was exported in the binary but not referenced anywhere.
static void ErrorHandler(int signr){
	error("ErrorHandler: SigNr. %d: %s\n", signr, strsignal(signr));
	EndGame();
	logout_all_players();
	exit(EXIT_FAILURE);
}
#endif

// Async-signal-safe hex printer for crash diagnostics.
static void write_hex(int fd, uintptr_t val) {
	char buf[18];
	buf[0] = '0';
	buf[1] = 'x';
	for (int i = 15; i >= 0; i--) {
		int nibble = val & 0xf;
		buf[2 + i] = (nibble < 10) ? ('0' + nibble) : ('a' + nibble - 10);
		val >>= 4;
	}
	write(fd, buf, 18);
}

static void CrashHandler(int signr, siginfo_t *info, void *ucontext_raw) {
	char buf[256];
	int len = snprintf(buf, sizeof(buf), "FATAL: Caught signal %d (%s) on tid %d\n",
					   signr, strsignal(signr), (int)gettid());
	if (len > 0) write(STDERR_FILENO, buf, len);

	// Print the faulting address.
	if (info) {
		len = snprintf(buf, sizeof(buf), "Faulting address: %p\n", info->si_addr);
		if (len > 0) write(STDERR_FILENO, buf, len);
	}

	// Print instruction pointer from ucontext.
	if (ucontext_raw) {
		ucontext_t *uc = (ucontext_t *)ucontext_raw;
		uintptr_t pc = (uintptr_t)uc->uc_mcontext.gregs[REG_RIP];
		const char *msg = "Instruction pointer: ";
		write(STDERR_FILENO, msg, 21);
		write_hex(STDERR_FILENO, pc);
		write(STDERR_FILENO, "\n", 1);
	}

	// Dump /proc/self/maps for addr2line usage.
	const char *maps_header = "--- /proc/self/maps ---\n";
	write(STDERR_FILENO, maps_header, 24);
	int maps_fd = open("/proc/self/maps", O_RDONLY);
	if (maps_fd >= 0) {
		char mbuf[4096];
		ssize_t n;
		while ((n = read(maps_fd, mbuf, sizeof(mbuf))) > 0) {
			write(STDERR_FILENO, mbuf, n);
		}
		close(maps_fd);
	}

	// Re-raise with default handler to get core dump
	signal(signr, SIG_DFL);
	raise(signr);
}

static void InitSignalHandler(void) {
	int Count = 0;
	Count += (SigHandler(SIGHUP, SigHupHandler) != SIG_ERR);
	Count += (SigHandler(SIGINT, DefaultHandler) != SIG_ERR);
	Count += (SigHandler(SIGQUIT, DefaultHandler) != SIG_ERR);
	Count += (SigHandler(SIGABRT, SigAbortHandler) != SIG_ERR);
	// Use SA_SIGINFO for crash signals to get faulting address and registers.
	{
		struct sigaction sa = {};
		sa.sa_sigaction = CrashHandler;
		sigemptyset(&sa.sa_mask);
		sa.sa_flags = SA_RESTART | SA_SIGINFO;
		Count += (sigaction(SIGSEGV, &sa, NULL) == 0);
		Count += (sigaction(SIGBUS, &sa, NULL) == 0);
		Count += (sigaction(SIGFPE, &sa, NULL) == 0);
		Count += (sigaction(SIGILL, &sa, NULL) == 0);
	}
	Count += (SigHandler(SIGUSR1, SIG_IGN) != SIG_ERR);
	Count += (SigHandler(SIGUSR2, SIG_IGN) != SIG_ERR);
	Count += (SigHandler(SIGPIPE, SIG_IGN) != SIG_ERR);
	Count += (SigHandler(SIGALRM, SIG_IGN) != SIG_ERR);
	Count += (SigHandler(SIGTERM, DefaultHandler) != SIG_ERR);
	Count += (SigHandler(SIGSTKFLT, SIG_IGN) != SIG_ERR);
	Count += (SigHandler(SIGCHLD, SIG_IGN) != SIG_ERR);
	Count += (SigHandler(SIGTSTP, SIG_IGN) != SIG_ERR);
	Count += (SigHandler(SIGTTIN, SIG_IGN) != SIG_ERR);
	Count += (SigHandler(SIGTTOU, SIG_IGN) != SIG_ERR);
	Count += (SigHandler(SIGURG, SIG_IGN) != SIG_ERR);
	Count += (SigHandler(SIGXCPU, DefaultHandler) != SIG_ERR);
	Count += (SigHandler(SIGXFSZ, DefaultHandler) != SIG_ERR);
	Count += (SigHandler(SIGVTALRM, SIG_IGN) != SIG_ERR);
	Count += (SigHandler(SIGWINCH, SIG_IGN) != SIG_ERR);
	Count += (SigHandler(SIGPOLL, SIG_IGN) != SIG_ERR);
	Count += (SigHandler(SIGPWR, DefaultHandler) != SIG_ERR);
	print(1, "InitSignalHandler: %d signal handlers registered (expected=%d).\n", Count, 0x1c);
}

static void ExitSignalHandler(void) {
	// no-op
}

static void SigAlarmHandler(int SigNr) {
	SigAlarmCounter += (1 + timer_getoverrun(BeatTimer));
}

static void InitTime(void) {
	ASSERT(Beat > 0);
	SigAlarmCounter = 0;
	SigHandler(SIGALRM, SigAlarmHandler);

	struct sigevent SigEvent = {};
	SigEvent.sigev_notify = SIGEV_THREAD_ID;
	SigEvent.sigev_signo = SIGALRM;
	SigEvent.sigev_notify_thread_id = gettid();
	if (timer_create(CLOCK_MONOTONIC, &SigEvent, &BeatTimer) == -1) {
		error("InitTime: Failed to create beat timer: (%d) %s\n", errno, strerror(errno));
		throw "cannot create beat timer";
	}

	struct itimerspec TimerSpec = {};
	TimerSpec.it_interval.tv_sec = Beat / 1000;
	TimerSpec.it_interval.tv_nsec = (Beat % 1000) * 1000000;
	TimerSpec.it_value = TimerSpec.it_interval;
	if (timer_settime(BeatTimer, 0, &TimerSpec, NULL) == -1) {
		error("InitTime: Failed to start beat timer: (%d) %s\n", errno, strerror(errno));
		throw "cannot start beat timer";
	}
}

static void ExitTime(void) {
	if (timer_delete(BeatTimer) == -1) {
		error("ExitTime: Failed to delete beat timer: (%d) %s\n", errno, strerror(errno));
	}

	SigHandler(SIGALRM, SIG_IGN);
}

static void UnlockGame(void) {
	// TODO(fusion): Probably use snprintf to format file name?
	char FileName[4096];
	strcpy(FileName, SAVEPATH);
	strcat(FileName, "/game.pid");

	std::ifstream InputFile(FileName, std::ios_base::in);
	if (!InputFile.fail()) {
		int Pid;
		InputFile >> Pid;

		if (Pid == getpid()) {
			unlink(FileName);
		}
	}
}

static void LockGame(void) {
	// TODO(fusion): Probably use snprintf to format file name?
	char FileName[4096];
	strcpy(FileName, SAVEPATH);
	strcat(FileName, "/game.pid");

	{
		std::ifstream InputFile(FileName, std::ios_base::in);
		if (!InputFile.fail()) {
			int Pid;
			InputFile >> Pid;
			if (Pid != 0) {
				throw "Game-Server is already running, PID file exists.";
			}
		}
	}

	{
		std::ofstream OutputFile(FileName, std::ios_base::out | std::ios_base::trunc);
		OutputFile << getpid();
	}

	atexit(UnlockGame);
}

void LoadWorldConfig(void) {
	TQueryManagerConnection Connection(kb(16));
	if (!Connection.isConnected()) {
		error("LoadWorldConfig: Cannot connect to query manager.\n");
		throw "cannot connect to querymanager";
	}

	int HelpWorldType;
	int HelpGameAddress[4];
	int ConfigGamePort = GamePort; // Preserve value from .tibia config (0 if not set)
	int Ret = Connection.loadWorldConfig(&HelpWorldType, &RebootTime, HelpGameAddress, &GamePort, &MaxPlayers,
										 &PremiumPlayerBuffer, &MaxNewbies, &PremiumNewbieBuffer);
	if (Ret != 0) {
		error("LoadWorldConfig: Cannot retrieve configuration data.\n");
		throw "cannot load world config";
	}

	// If GamePort was explicitly set in .tibia config, use that instead of the
	// database value. The database port is only for the login character list.
	if (ConfigGamePort != 0) {
		GamePort = ConfigGamePort;
	}

	WorldType = (TWorldType)HelpWorldType;
	snprintf(GameAddress, sizeof(GameAddress), "%d.%d.%d.%d", HelpGameAddress[0], HelpGameAddress[1],
			 HelpGameAddress[2], HelpGameAddress[3]);
}

static void InitAll(void) {
	try {
		ReadConfig();
		SetQueryManagerLoginData(1, WorldName);
		LoadWorldConfig();
		InitSHM(!BeADaemon);
		LockGame();
		init_log("game");
		srand(time(NULL));
		InitSignalHandler();
		init_connections();
		init_communication();
		InitStrings();
		init_writer();
		init_reader();
		init_objects();
		init_map();
		init_info();
		init_move_use();
		init_magic();
		init_cr();
		init_houses();
		InitTime();
		apply_patches();
	} catch (const char *str) {
		error("Initialization error: %s\n", str);
		exit(EXIT_FAILURE);
	}
}

static void ExitAll(void) {
	EndGame();
	ExitTime();
	exit_cr();
	exit_magic();
	exit_move_use();
	exit_info();
	exit_houses();
	exit_map(SaveMapOn);
	exit_objects();
	exit_reader();
	exit_writer();
	ExitStrings();
	exit_communication();
	exit_connections();
	ExitSignalHandler();
	ExitSHM();
}

static void ProcessCommand(void) {
	int Command = GetCommand();
	if (Command != 0) {
		char *Buffer = GetCommandBuffer();
		if (Command == 1) {
			if (Buffer != NULL) {
				broadcast_message(TALK_ADMIN_MESSAGE, "%s", Buffer);
			} else {
				error("ProcessCommand: Text for broadcast is NULL.\n");
			}
		} else {
			error("ProcessCommand: Unknown command %d.\n", Command);
		}

		SetCommand(0, NULL);
	}
}

static void AdvanceGame(int Delay) {
	static int CreatureTimeCounter = 0;
	static int CronTimeCounter = 0;
	static int SkillTimeCounter = 0;
	static int OtherTimeCounter = 0;
	static int OldAmbiente = -1;
	static uint32 NextMinute = 30;
	static bool Lag = false;

	CreatureTimeCounter += Delay;
	CronTimeCounter += Delay;
	SkillTimeCounter += Delay;
	OtherTimeCounter += Delay;

	if (CreatureTimeCounter >= 1750) {
		CreatureTimeCounter -= 1000;
		process_creatures();
	}

	if (CronTimeCounter >= 1500) {
		CronTimeCounter -= 1000;
		process_cron_system();
	}

	if (SkillTimeCounter >= 1250) {
		SkillTimeCounter -= 1000;
		process_skills();
	}

	if (OtherTimeCounter >= 1000) {
		OtherTimeCounter -= 1000;

		RoundNr += 1;
		SetRoundNr(RoundNr);

		process_connections();
		process_monsterhomes();
		process_monster_raids();
		process_communication_control();
		process_reader_thread_replies(refresh_sector, send_mails);
		process_writer_thread_replies();
		ProcessCommand();

		// TODO(fusion): Shouldn't we be checking both brightness and color?
		int Brightness, Color;
		GetAmbiente(&Brightness, &Color);
		if (OldAmbiente != Brightness) {
			OldAmbiente = Brightness;
			TConnection *Connection = get_first_connection();
			while (Connection != NULL) {
				if (Connection->live()) {
					send_ambiente(Connection);
				}
				Connection = get_next_connection();
			}
		}

		if (RoundNr % 10 == 0) {
			net_load_check();
		}

		if (RoundNr >= NextMinute) {
			int Hour, Minute;
			GetRealTime(&Hour, &Minute);

			refresh_cylinders();
			if (Minute % 5 == 0) {
				create_player_list(true);
			}
			if (Minute % 15 == 0) {
				save_player_data_order();
			}
			if (Minute == 0) {
				net_load_summary();
			}
			if (Minute == 55) {
				write_kill_statistics();
			}

			int RealTime = Minute + Hour * 60;
			if ((RealTime + 5) % 1440 == RebootTime) {
				if (Reboot) {
					broadcast_message(TALK_ADMIN_MESSAGE,
									  "Server is saving game in 5 minutes.\nPlease come back in 10 minutes.");
				} else {
					broadcast_message(TALK_ADMIN_MESSAGE, "Server is going down in 5 minutes.\nPlease log out.");
				}
				CloseGame();
			} else if ((RealTime + 3) % 1440 == RebootTime) {
				if (Reboot) {
					broadcast_message(TALK_ADMIN_MESSAGE,
									  "Server is saving game in 3 minutes.\nPlease come back in 10 minutes.");
				} else {
					broadcast_message(TALK_ADMIN_MESSAGE, "Server is going down in 3 minutes.\nPlease log out.");
				}
			} else if ((RealTime + 1) % 1440 == RebootTime) {
				if (Reboot) {
					broadcast_message(TALK_ADMIN_MESSAGE, "Server is saving game in one minute.\nPlease log out.");
				} else {
					broadcast_message(TALK_ADMIN_MESSAGE, "Server is going down in one minute.\nPlease log out.");
				}
			} else if (RealTime == RebootTime) {
				CloseGame();
				logout_all_players();
				send_all();
				if (Reboot) {
					refresh_map();
				}
				save_map();
				SaveMapOn = false;
				EndGame();
			}

			NextMinute = GetRoundForNextMinute();
		}
		CleanupDynamicStrings();
	}

	if (Delay > Beat) {
		log_message("lag", "Delay %d msec.\n", Delay);
	}

	// TODO(fusion): Why would we delay creature movement yet another beat?
	if (Delay < 1000) {
		move_creatures(Delay);
		Lag = false;
	} else {
		if (!Lag && RoundNr > 10) {
			error("AdvanceGame: No creature movement due to lag (delay: %d msec).\n", Delay);
		}
		Lag = true;
	}

	send_all();
}

static void SigUsr1Handler(int signr) {
	SigUsr1Counter += 1;
}

static void LaunchGame(void) {
	SaveMapOn = true;
	SigUsr1Counter = 0;
	SigAlarmCounter = 0;

	SigBlock(SIGUSR1);
	SigHandler(SIGUSR1, SigUsr1Handler);
	StartGame();

	print(1, "LaunchGame: Game server is ready (Pid=%d, Tid=%d).\n", getpid(), gettid());

	// IMPORTANT(fusion): In general signal handlers can execute on any thread in
	// the process group but the design of the server is to use signals directed
	// at different threads to communicate certain events (see `CommunicationThread`
	// for example).
	//	Even if that wasn't the case, loads/stores on x86 are always ATOMIC and when
	// there is a single writer (signal handlers), even a read-modify-write will be
	// atomic.
	//	This is to say, there should be no problem with reading from `SigUsr1Counter`,
	// `SigAlarmCounter`, or `SaveMapOn`, which may be modified from signal handlers.

	while (GameRunning()) {
		while (SigUsr1Counter == 0 && SigAlarmCounter == 0) {
			SigWaitAny();
		}

		if (SigUsr1Counter > 0) {
			SigUsr1Counter = 0;
			receive_data();
		}

		int NumBeats = SigAlarmCounter;
		if (NumBeats > 0) {
			SigAlarmCounter = 0;
			AdvanceGame(NumBeats * Beat);
		}
	}

	logout_all_players();
}

static bool DaemonInit(bool NoFork) {
	if (!NoFork) {
		pid_t Pid = fork();
		if (Pid < 0) {
			return true;
		}

		if (Pid != 0) {
			exit(EXIT_SUCCESS);
		}

		setsid();
	}

	umask(0177);
	chdir(SAVEPATH);

	int OpenMax = sysconf(_SC_OPEN_MAX);
	if (OpenMax < 0) {
		OpenMax = 1024;
	}

	for (int fd = 0; fd < OpenMax; fd += 1) {
		close(fd);
	}

	return false;
}

int main(int argc, char **argv) {
	bool NoFork = false;
	BeADaemon = false;
	Reboot = true;

	for (int i = 1; i < argc; i += 1) {
		if (strcmp(argv[i], "daemon") == 0) {
			BeADaemon = true;
		} else if (strcmp(argv[i], "nofork") == 0) {
			NoFork = true;
		}
	}

	// TODO(fusion): It doesn't make sense for `DaemonInit` to even return here.
	// It either exits the parent or child process, or let it run.
	if (BeADaemon && DaemonInit(NoFork)) {
		return 2;
	}

	puts("Tibia Game-Server\n(c) by CIP Productions, 2003.\n");

	InitAll();
	atexit(ExitAll);

	// TODO(fusion): The original binary does use exceptions but identifying
	// try..catch blocks are not as straightforward as throw statements. I'll
	// leave this one at the top level but we should come back to this problem
	// once we identify all throw statements and how to roughly handle them.
	try {
		LaunchGame();
	} catch (RESULT r) {
		error("main: Uncaught exception %d.\n", r);
	} catch (const char *str) {
		error("main: Uncaught exception \"%s\".\n", str);
	} catch (const std::exception &e) {
		error("main: Uncaught exception %s.\n", e.what());
	} catch (...) {
		error("main: Uncaught exception of unknown type.\n");
	}

	if (!Reboot) {
		print(1, "Shutting down game server...\n");
	} else {
		UnlockGame();

		char FileName[4096];
		snprintf(FileName, sizeof(FileName), "%s/reboot-daily", BINPATH);
		if (FileExists(FileName)) {
			ExitAll();
			print(1, "Restarting game server...\n");
			execv(FileName, argv);
		} else {
			print(1, "Reboot script does not exist.\n");
		}
	}

	return 0;
}
