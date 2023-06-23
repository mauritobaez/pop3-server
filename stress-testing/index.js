const Pop3Command = require('node-pop3');
const NetcatClient = require('netcat/client');

const clients = 900;
let finished = 0;

async function addUsers(users) {
    const nc = new NetcatClient();
    return new Promise((resolve) => {
        const client = nc.addr('::1').port(2110).connect().send('a root root\r\n').on('data', (buffer) => {
        });
        const total = { count: 0 };
        for (let i = 0; i < users; i += 1) {
            client.send(`u+ ${i} ${i}\r\n`, () => {
                total.count += 1;
                if (total.count === users) {
                    client.close();
                    resolve();
                }
            })
        }
        client.send('q\r\n');
    });
}

function parseNanoToSeconds(hrtime) {
    var seconds = (hrtime[0] + (hrtime[1] / 1e9));
    return seconds;
}

async function getEmail(index) {
    const pop3 = new Pop3Command({
        user: index,
        password: index,
        host: "localhost",
        port: 1110
    });

    const label = `client ${index}`;
    const start = process.hrtime();
    await pop3.RETR(1)    
    finished += 1;
    console.log("Finished", finished);
    const seconds = parseNanoToSeconds(process.hrtime(start));
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
        emails.push(getEmail(i));
    }
    const seconds = await Promise.all(emails);
    console.log(seconds.reduce((prev, curr) => (prev + curr)) / seconds.length);
}

start();