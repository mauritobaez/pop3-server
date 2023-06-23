const Pop3Command = require('node-pop3');
const NetcatClient = require('netcat/client');

const clients = 900;
let finished = 0;
let totaltime = 0;

function addUsers(users) {
    const regex = new RegExp(/[+-]/, "g");
    const nc = new NetcatClient();
    return new Promise((resolve) => {
        const total = { count: 0 };
        let responses = 0;

        const client = nc.addr('::1').port(2110).connect().send('a root root\r\n').on('data', (a) => {
            const b = (a.toString().match(regex));
            responses += (b || []).length;
            if (total.count < responses) {
                client.send('q\r\n', () => {
                    client.close();
                    resolve();
                });
            }
        });
        for (let i = 0; i < users; i += 1) {
            client.send(`u+ ${i} ${i}\r\n`, () => {
                total.count += 1;
            });
        }
    });
}

function sleeper(ms) {
    return function(x) {
        return new Promise(resolve => setTimeout(() => resolve(x), ms));
    };
}

function parseNanoToMSeconds(hrtime) {
    var seconds = (hrtime[0] + (hrtime[1] / 1e6));
    return seconds;
}

async function getEmail(index) {
    const pop3 = new Pop3Command({
        user: index,
        password: index,
        host: "localhost",
        port: 1110
    });
    const start = process.hrtime();

    await pop3.connect();
    finished += 1;
    // await sleeper(5 * 1000)();
    await pop3.command('USER', index);
    await pop3.command('PASS', index);
    await pop3.command('RETR', 1);
    
    await pop3.command('QUIT');
    const seconds = parseNanoToMSeconds(process.hrtime(start));
    totaltime += seconds;
    console.log(totaltime / finished);
    return seconds;
}

async function start() {
    let connections;
    try {
        connections = parseInt(process.argv.at(2), 10);
    } catch (e) {
        connections = clients;
    }
    await addUsers(connections);
    const emails = [];
    for (let i = 0; i < connections; i += 1) {
        setTimeout(() => {
            emails.push(getEmail(i));
        }, i * 5);
    }
    await Promise.all(emails);
}

start();